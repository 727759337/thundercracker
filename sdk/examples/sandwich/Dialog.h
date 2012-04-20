#pragma once

#include "Common.h"

class Dialog {
private:
    VideoBuffer* mCube;
    Short2 mPosition;

public:
    void Init(VideoBuffer *cube);
    void ShowAll(const char* lines);
    const char* Show(const char* msg);
    void Erase();
    void SetAlpha(uint8_t alpha);

private:    
    void DrawGlyph(char ch);
    unsigned MeasureGlyph(char ch);
    void DrawText(const char* msg);
    void MeasureText(const char *str, unsigned *outCount, unsigned *outPx);

};
