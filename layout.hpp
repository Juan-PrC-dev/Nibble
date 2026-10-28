#pragma once
#include <algorithm>

// Artwork coordinates stay at their original design size; only the widget is scaled.
// Settings and summary windows use their normal Windows DPI scale.
namespace WidgetLayout {
constexpr int DesignWidth=320, DesignHeight=260;
constexpr int ScaleNumerator=3, ScaleDenominator=5;
constexpr int LogicalWidth=DesignWidth*ScaleNumerator/ScaleDenominator;
constexpr int LogicalHeight=DesignHeight*ScaleNumerator/ScaleDenominator;
constexpr int roundedDivide(long long value,int divisor){return int(value>=0?(value+divisor/2)/divisor:-((-value+divisor/2)/divisor));}
constexpr int designPixels(int value,int dpi){return roundedDivide((long long)value*dpi*ScaleNumerator,96*ScaleDenominator);}
constexpr int logicalPixels(int value,int dpi){return roundedDivide((long long)value*dpi,96);}
constexpr int fontPixels(int designSize,int dpi){return std::max(designPixels(designSize,dpi),logicalPixels(8,dpi));}
}
