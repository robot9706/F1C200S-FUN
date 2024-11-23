#pragma once

#include <stdint.h>

void printChar(char chr);
void printStr(const char* str);
void print8(uint8_t u8);
void print16(uint16_t u16);
void print32(uint32_t u32);
void print64(uint64_t u64);
void printDec8(uint8_t u8);
void printDec16(uint16_t u8);
void printf(const char* fmt, ...);