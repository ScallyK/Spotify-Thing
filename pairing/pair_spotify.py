#!/usr/bin/env python3
"""Spotify PKCE pairing utility for the Spotify Thing."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import secrets
import sys
import time
import webbrowser

from http.server import BaseHTTPRequestHandler, HTTPServer

from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, urlencode, urlparse
from urllib.request import Request, urlopen

AUTHORIZE_URL = "https://accounts.spotify.com/authorize"
TOKEN_URL = "https://accounts.spotify.com/api/token"

# Spotify must redirect to the same loopback address used by the temporary local server.
REDIRECT_URI = "http://127.0.0.1:8765/callback"

# The firmware only needs permission to read and control Spotify playback.
SCOPES = "user-read-playback-state user-modify-playback-state"

class CallbackHandler(BaseHTTPRequestHandler):

    # Authorization codes and state values must never be written to the request log.
    def log_message(self, message_format: str, *arguments: object) -> None:
        return

    # Spotify redirects the browser here after the account owner approves or cancels pairing.
    def do_GET(self) -> None:  # noqa: N802

        callbackValues = parse_qs(urlparse(self.path).query)

        # The waiting main thread reads this result after the browser completes the redirect.
        self.server.result = {
            "code": callbackValues.get("code", [""])[0],
            "state": callbackValues.get("state", [""])[0],
            "error": callbackValues.get("error", [""])[0],
        }

        callbackMessage = (
            "<h1>Spotify paired</h1><p>You may close this browser window.</p>"
            if not self.server.result["error"]
            else "<h1>Spotify pairing was cancelled</h1><p>Return to the terminal.</p>"
        )

        encodedResponseBody = ("<!doctype html><meta charset=utf-8>" + callbackMessage).encode(
            "utf-8"
        )

        self.send_response(200)
        # Return a short local confirmation page without exposing the authorization code.
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(encodedResponseBody)))
        self.end_headers()

        self.wfile.write(encodedResponseBody)

# A cryptographically random verifier proves this terminal initiated the browser request.
def create_pkce_verifier() -> str:
    return base64.urlsafe_b64encode(secrets.token_bytes(64)).decode("ascii").rstrip("=")

def request_token(formFields: dict[str, str]) -> dict[str, object]:
    
    # Spotify expects OAuth token requests as form-encoded POST bodies.
    tokenRequest = Request(
        TOKEN_URL,
        data=urlencode(formFields).encode("utf-8"),
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        method="POST",
    )

    try:
        with urlopen(tokenRequest, timeout=20) as tokenResponse:
            return json.loads(tokenResponse.read().decode("utf-8"))

    except HTTPError as error:
        # Include Spotify's response body because it usually identifies configuration mistakes.
        errorResponseBody = error.read().decode("utf-8", errors="replace")
        raise RuntimeError(
            f"Spotify token exchange failed ({error.code}): {errorResponseBody}"
        ) from error

    except URLError as error:
        raise RuntimeError(f"Could not reach Spotify: {error.reason}") from error


def main() -> None:

    argumentParser = argparse.ArgumentParser(description="Pair Spotify Thing with PKCE.")
    argumentParser.add_argument("--client-id", help="Spotify app client ID (not a secret)")

    commandLineArguments = argumentParser.parse_args()

    clientId = commandLineArguments.client_id or input("Spotify client ID: ").strip()

    if not clientId:
        raise SystemExit("A Spotify client ID is required.")

    codeVerifier = create_pkce_verifier()

    # S256 binds the browser authorization code to the secret verifier retained by this process.
    codeChallenge = base64.urlsafe_b64encode(
        hashlib.sha256(codeVerifier.encode("ascii")).digest()
    ).decode("ascii").rstrip("=")

    # This random state value lets the callback reject a forged browser redirect.
    authorizationState = secrets.token_urlsafe(32)
    authorizationQuery = urlencode(
        {
            "client_id": clientId,
            "response_type": "code",
            "redirect_uri": REDIRECT_URI,
            "scope": SCOPES,
            "state": authorizationState,
            "code_challenge_method": "S256",
            "code_challenge": codeChallenge,
        }
    )

    # Bind only to loopback so another device on the network cannot receive the callback.
    callbackServer = HTTPServer(("127.0.0.1", 8765), CallbackHandler)
    
    # A short timeout keeps the loop responsive while it waits for the browser callback.
    callbackServer.timeout = 1
    callbackServer.result = None

    print("Opening Spotify sign-in in your browser…")
    print("If it does not open, visit this URL:\n" + AUTHORIZE_URL + "?" + authorizationQuery)

    webbrowser.open(AUTHORIZE_URL + "?" + authorizationQuery)

    # Pairing ends after five minutes instead of leaving the local callback server open.
    authorizationDeadline = time.monotonic() + 300

    # Handle one callback at a time until Spotify redirects back or the authorization window expires.

    while callbackServer.result is None and time.monotonic() < authorizationDeadline:
        callbackServer.handle_request()

    # Close the loopback listener as soon as pairing succeeds, fails, or times out.
    callbackServer.server_close()

    if callbackServer.result is None:
        raise SystemExit("Timed out waiting for Spotify. Run the utility again.")

    if callbackServer.result["error"]:
        raise SystemExit("Spotify authorization failed: " + callbackServer.result["error"])

    # Compare state values in constant time before accepting the authorization code.
    if not secrets.compare_digest(callbackServer.result["state"], authorizationState) or not callbackServer.result["code"]:
        raise SystemExit("Authorization state did not match. Run the utility again.")

    tokenResponse = request_token(
        {
            "client_id": clientId,
            "grant_type": "authorization_code",
            "code": callbackServer.result["code"],
            "redirect_uri": REDIRECT_URI,
            "code_verifier": codeVerifier,
        }
    )
    
    # The first exchange yields the credential the ESP32 needs after each access token expires.
    refreshToken = tokenResponse.get("refresh_token")

    if not isinstance(refreshToken, str) or not refreshToken:
        raise SystemExit("Spotify did not return a refresh token. Check the app's redirect URI and retry.")

    # Validate the token before asking the user to place it on the ESP32.
    validationResponse = request_token(
        {
            "client_id": clientId,
            "grant_type": "refresh_token",
            "refresh_token": refreshToken,
        }
    )

    if not isinstance(validationResponse.get("access_token"), str):
        raise SystemExit("Spotify returned no access token while validating the refresh token.")

    rotatedRefreshToken = validationResponse.get("refresh_token")

    if isinstance(rotatedRefreshToken, str) and rotatedRefreshToken:
        
        # Use Spotify's rotation so later refreshes do not rely on the invalidated credential.
        refreshToken = rotatedRefreshToken

    # Keep the refresh token in the terminal output so the user controls where it is stored.
    print("\nPairing and refresh-token validation succeeded.")
    print("This next line is a secret. Do not share it or commit it.")
    print(f'#define SPOTIFY_REFRESH_TOKEN "{refreshToken}"')
    print("\nCopy it into firmware/include/secrets.h, then upload the firmware.")


if __name__ == "__main__":
    main()