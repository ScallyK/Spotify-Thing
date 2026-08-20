#include "util/TextUtils.h"

// The built-in GFX font only supports printable ASCII characters.
String TextUtils::sanitizeForBuiltinFont(const String &textValue)
{
  String sanitizedText;

  for (size_t characterIndex = 0; characterIndex < textValue.length();)

  {
    // Keep ASCII, replace each UTF-8 sequence once, and discard control bytes.
    const uint8_t currentByte = static_cast<uint8_t>(textValue[characterIndex]);

    if (currentByte >= 32 && currentByte <= 126){
      sanitizedText += static_cast<char>(currentByte);
      ++characterIndex;
    }

    else if (currentByte >= 128){
      sanitizedText += '?';
      ++characterIndex;

      while (characterIndex < textValue.length() &&
             (static_cast<uint8_t>(textValue[characterIndex]) & 0xC0) == 0x80)
      {
        ++characterIndex;
      }

    }

    else{
      ++characterIndex;
    }

  }

  return sanitizedText;

}

uint16_t TextUtils::measureTextWidth(Adafruit_GFX &displayDriver, const String &textValue,
                                     uint8_t textSize)
{

  // GFX calculates bounds using its current font and text scale.
  int16_t boundsX = 0;
  int16_t boundsY = 0;
  uint16_t boundsWidth = 0;
  uint16_t boundsHeight = 0;

  displayDriver.setTextSize(textSize);
  displayDriver.getTextBounds(textValue, 0, 0, &boundsX, &boundsY, &boundsWidth, &boundsHeight);

  return boundsWidth;
  
}

String TextUtils::clipToWidth(Adafruit_GFX &displayDriver, const String &textValue,
                              uint16_t maximumWidth, uint8_t textSize)
{

  if (!textValue.length()){
    return "";
  }

  String clippedText;

  for (size_t characterIndex = 0; characterIndex < textValue.length(); ++characterIndex)
  {

    // Grow one character at a time because proportional fonts cannot be safely approximated.
    const String candidateText = clippedText + textValue[characterIndex];

    if (measureTextWidth(displayDriver, candidateText, textSize) <= maximumWidth){
      clippedText = candidateText;
    }

    else{
      break;
    }

  }

  if (clippedText.length() >= textValue.length()){
    return clippedText;
  }

  while (clippedText.length() &&
         measureTextWidth(displayDriver, clippedText + "...", textSize) > maximumWidth)
  {
    // Reserve enough room for the ellipsis before returning a shortened title.
    clippedText.remove(clippedText.length() - 1);
  }

  return clippedText.length() ? clippedText + "..." : "...";

}

String TextUtils::formatPlaybackTime(int milliseconds)
{

  // Negative progress is displayed as the start of the track instead of a signed time.
  const int playbackSeconds = max(0, milliseconds / 1000);

  char formattedTime[12];

  snprintf(formattedTime, sizeof(formattedTime), "%d:%02d", playbackSeconds / 60,
           playbackSeconds % 60);

  return String(formattedTime);

}