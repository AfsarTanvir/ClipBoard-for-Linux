#pragma once

#include <QColor>

// Dark colour palette, modelled on the Windows 11 clipboard popup.
namespace Theme {

inline const QColor Window{0x20, 0x20, 0x20};
inline const QColor Border{0x3a, 0x3a, 0x3a};
inline const QColor Card{0x2b, 0x2b, 0x2b};
inline const QColor CardHover{0x33, 0x33, 0x33};
inline const QColor CardSelected{0x3a, 0x3a, 0x3a};
inline const QColor Badge{0x40, 0x40, 0x40};
inline const QColor Text{0xf3, 0xf3, 0xf3};
inline const QColor TextDim{0x9a, 0x9a, 0x9a};
inline const QColor Accent{0x4c, 0xc2, 0xff};
inline const QColor Danger{0xff, 0x6b, 0x6b};

} // namespace Theme
