#pragma once

// 线条形态标识：直线 / 弧线 / 正弦。菜单图标与插入分派共用这一个枚举，
// 图元自身的类型标识仍由各类的 Kind() 给出。
enum class LineGlyph { STRAIGHT = 0, ARC, ANGLE, SINE };
