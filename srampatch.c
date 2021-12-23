/* undefine if you want the original buggy behaviour for EEPROM type 1 roms like 0002,
   which causes the output rom to get truncated to 0 on windows, and a segfault due
   to double-free anywhere else. */
#define BUGFIX

#ifndef _WIN32
#define __cdecl
#endif

#define _GNU_SOURCE
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define qmemcpy memcpy

#define _BYTE  uint8_t
#define _WORD  uint16_t
#define _DWORD uint32_t
#define LOBYTE(x)   (*((_BYTE*)&(x)))   // low byte
#define LOWORD(x)   (*((_WORD*)&(x)))   // low word
#define LODWORD(x)  (*((_DWORD*)&(x)))  // low dword
#define HIBYTE(x)   (*((_BYTE*)&(x)+1))
#define HIWORD(x)   (*((_WORD*)&(x)+1))
#define HIDWORD(x)  (*((_DWORD*)&(x)+1))
#define BYTEn(x, n)   (*((_BYTE*)&(x)+n))
#define WORDn(x, n)   (*((_WORD*)&(x)+n))
#define BYTE1(x)   BYTEn(x,  1)         // byte 1 (counting from 0)
#define BYTE2(x)   BYTEn(x,  2)
#define BYTE3(x)   BYTEn(x,  3)
#define BYTE4(x)   BYTEn(x,  4)

//-------------------------------------------------------------------------
// Function declarations

int get_device_type_401000();
char __cdecl set_device_type_401010(char a1);
int print_device_list_401020();
void __cdecl get_savetype_401050(unsigned char *filecontents, long filesize, int *backuptype, int *version); // idb
bool __cdecl write_rom_401100(void *data, size_t size, char *fn_out);
bool __cdecl rom_to_mem_401150(unsigned char **a1, long *filesize, char *filename); // idb
unsigned char *__cdecl find_mem_4011E0(unsigned char *haystack, long haystacklen, const void *needle, int needlelen);
char *__cdecl get_flash_data_a_hunk_401230(int hunkno, unsigned int *hunklen); // idb
long __cdecl find_flash_data_a_hunk_401250(unsigned char *rom, long romlen, int hunkno);
char __cdecl rom_insert_flash_data_a_hunk_401290(unsigned char *rom, long romlen, long romoff, int hunkno); // idb
bool __cdecl flash_data_a_hunk_replace_4012D0(unsigned char *rom, long romlen, int searchhunk, int replacehunk);
bool __cdecl eeprom_type_a_401340(unsigned char *rom, unsigned long romlen);
bool __cdecl eeprom_type_b_4014D0(unsigned char *rom, long romlen);
bool __cdecl eeprom_type_c_401520(unsigned char *rom, long romlen);
bool __cdecl eeprom_patch_401570(int version, unsigned char *rom, long romlen);
bool __cdecl flash_type_b_401600(unsigned char *a1, long a2);
bool __cdecl flash_type_c_401660(unsigned char *rom, long romlen);
bool __cdecl flash_type_d_4016C0(unsigned char *rom, long romlen);
bool __cdecl flash_type_a_401720(unsigned char *rom, long romlen);
bool __cdecl flash_patch_401810(int version, unsigned char *rom, long romlen);
bool __cdecl patch_rom_4018C0(char *fn_in, char *fn_out);

//-------------------------------------------------------------------------
// Data declarations

/* HACK: the eeprom_type_a_401340 routine needs to pad the rom with
   additional data. therefore it allocates new memory but then fails
   to passes it back to the calling sites. store the new rom location
   in this variable */
static void *newrom = 0;
static long newromsize = 0;

static const char * savetype_list_40A040[6] = {
  "EEPROM_V",
  "FLASH_V",
  "SRAM_V",
  "SRAM_F_V",
  "FLASH1M_V",
  "FLASH512_V"
}; // 0x40a040

static const unsigned char
backuptype_tab_40A058[] = "\x03\x02\x01\x01\x02\x02";

static const char *backuptype_names_40A060[] = {
  "None",
  "Sram",
  "Flash",
  "Eeprom",
};

/* the 2 hunks using the compound literal syntax were modified this way
   as the buggy eeprom_type_a_401340() function overwrites them with
   some offsets. */
unsigned char *flash_data_a_tab[40] = {
  "\xd0\x20\x00\x05\x01\x88\x01\x22\x08\x1c\x10\x40\x02\x1c\x11\x04",
  "\xe0\x20\x00\x05\x01\x88\x01\x22\x08\x1c\x10\x40\x02\x1c\x11\x04\x08\x0c\x00\x28\x01\xd0\x1b\xe0",
  (unsigned char[]){"\x39\x68\x27\x48\x81\x42\x23\xd0\x89\x1c\x08\x88\x01\x28\x02\xd1\x24\x48\x78\x60\x33\xe0\x00\x23\x00\x22\x89\x1c\x10\xb4\x01\x24\x08\x68\x20\x40\x5b\x00\x03\x43\x89\x1c\x52\x1c\x06\x2a\xf7\xd1\x10\xbc\x39\x60\xdb\x01\x02\x20\x00\x02\x1b\x18\x0e\x20\x00\x06\x1b\x18\x7b\x60\x39\x1c\x08\x31\x08\x88\x09\x38\x08\x80\x16\xe0\x15\x49\x00\x23\x00\x22\x10\xb4\x01\x24\x08\x68\x20\x40\x5b\x00\x03\x43\x89\x1c\x52\x1c\x06\x2a\xf7\xd1\x10\xbc\xdb\x01\x02\x20\x00\x02\x1b\x18\x0e\x20\x00\x06\x1b\x18\x08\x3b\x3b\x60\x0b\x48\x39\x68\x01\x60\x0a\x48\x79\x68\x01\x60\x0a\x48\x39\x1c\x08\x31\x0a\x88\x80\x21\x09\x06\x0a\x43\x02\x60\x07\x48\x00\x47\x00\x00\x00\x00\x00\x0d\x00\x00\x00\x0e\x04\x00\x00\x0e\xd4\x00\x00\x04\xd8\x00\x00\x04\xdc\x00\x00\x04\xff\xff\xff\x08"},
  "\x0e\x48\x39\x68\x01\x60\x0e\x48\x79\x68",
  (unsigned char[]){"\x00\x48\x00\x47\x01\xff\xff\x08\x79\x68"},
  "\x30\xb5\xa9\xb0\x0d\x1c\x00\x04\x04\x0c\x03\x48\x00\x68\x80\x88",
  "\x70\xb5\xa2\xb0\x0d\x1c\x00\x04\x03\x0c\x03\x48\x00\x68\x80\x88",
  "\x70\xb5\x00\x04\x0a\x1c\x40\x0b\xe0\x21\x09\x05\x41\x18\x07\x31\x00\x23\x10\x78\x08\x70\x01\x33\x01\x32\x01\x39\x07\x2b\xf8\xd9\x00\x20\x70\xbc\x02\xbc\x08\x47",
  "\x70\xb5\x00\x04\x0a\x1c\x40\x0b\xe0\x21\x09\x05\x41\x18\x07\x31\x00\x23\x08\x78\x10\x70\x01\x33\x01\x32\x01\x39\x07\x2b\xf8\xd9\x00\x20\x70\xbc\x02\xbc\x08\x47",
  "\x80\xb5\x94\xb0\x6f\x46\x79\x60\x39\x1c\x08\x80\x38\x1c\x01\x88",
  "\x7c\xb5\x90\xb0\x00\x03\x0a\x1c\xe0\x21\x09\x05\x09\x18\x01\x23\x1b\x03\x10\x78\x08\x70\x01\x3b\x01\x32\x01\x31\x00\x2b\xf8\xd1\x00\x20\x10\xb0\x7c\xbc\x08\xbc\x08\x47",
  "\x90\xb5\x93\xb0\x6f\x46\x39\x1d\x08\x1c\x00\xf0\xaf\xf9\x38\x1d",
  "\x00\xb5\x3d\x20\x00\x02\x1f\x21\x08\x43\x02\xbc\x08\x47",
  "\x80\xb5\x92\xb0\x6f\x46\x1f\x48\x1e\x49\x0d\x0a\x88\x1e\x4b\x11\x1c",
  "\x7c\xb5\x00\x07\x00\x0c\xe0\x21\x09\x05\x09\x18\x01\x23\x1b\x04\xff\x20\x08\x70\x01\x3b\x01\x31\x00\x2b\xfa\xd1\x00\x20\x7c\xbc\x02\xbc\x08\x47",
  "\x80\xb5\x94\xb0\x6f\x46\x39\x1c\x08\x80\x38\x1c\x01\x88\x0f\x29",
  "\x7c\xb5\x00\x07\x00\x0c\xe0\x21\x09\x05\x09\x18\x01\x23\x1b\x03\xff\x20\x08\x70\x01\x3b\x01\x31\x00\x2b\xfa\xd1\x00\x20\x7c\xbc\x02\xbc\x08\x47",
  "\xf0\xb5\x90\xb0\x0f\x1c\x00\x04\x04\x0c\x0f\x2c\x04\xd9\x01\x48",
  "\x70\xb5\x00\x03\x0a\x1c\xe0\x21\x09\x05\x41\x18\x01\x23\x1b\x03\x10\x78\x08\x70\x01\x3b\x01\x32\x01\x31\x00\x2b\xf8\xd1\x00\x20\x70\xbc\x02\xbc\x08\x47",
  "\xff\xf7\xaa\xff\x00\x04\x03\x0c\x03\x4a\x01\x24\x07\xe0\x00\x00",
  "\x1b\x23\x1b\x02\x32\x20\x03\x43",
  "\x70\xb5\x90\xb0\x15\x4d\x29\x88\x15\x4e\x31\x40\x15\x48\x00\x68",
  "\x00\x20\x70\x47",
  "\x70\xb5\x46\x46\x40\xb4\x90\xb0\x00\x04\x03\x0c\x0f\x2b\x3b\xd8",
  "\xf0\xb5\x90\xb0\x0f\x1c\x00\x04\x04\x0c\x03\x48\x00\x68\x40\x89",
  "\x7c\xb5\x90\xb0\x00\x03\x0a\x1c\xe0\x21\x09\x05\x09\x18\x01\x23\x1b\x03\x10\x78\x08\x70\x01\x3b\x01\x32\x01\x31\x00\x2b\xf8\xd1\x00\x20\x10\xb0\x7c\xbc\x02\xbc\x08\x47",
  "\xff\xf7\xaa\xfe\x0f\x20\x04\x40\x07\x4b\x01\x20\x43\x40\x6a\x46",
  "\xf0\xb5\xac\xb0\x0d\x1c\x00\x04\x01\x0c\x12\x06\x17\x0e\x03\x48",
  "\xaa\x21\x19\x70\x05\x4a\x55\x21\x11\x70\xb0\x21\x19\x70\xe0\x21",
  "\x10\x70\x05\x49\x55\x20\x08\x70\x90\x20\x10\x70\x10\xa9\x03\x4a",
  "\x14\x49\xaa\x24\x0c\x70\x13\x4b\x55\x22\x1a\x70\x80\x20\x08\x70",
  "\x55\x22\x1a\x70\x80\x20\x08\x70\x0d\x70\x1a\x70\x30\x20\x20\x70",
  "\x10\x70\x0b\x49\x55\x20\x08\x70\xa0\x20\x10\x70",
  "\x22\x70\x09\x4b\x55\x22\x1a\x70\xa0\x22\x22\x70",
  "\x80\x21\x09\x02\x09\x22\x12\x06\x9f\x44\x11\x80\x03\x49\xc3\x02\xc9\x18\x11\x80\x70\x47\xfe\xff\xff\x01\x00\x00\x00\x00",
  "\x00\x00\x05\x49\x55\x20\x00\x00\x90\x20\x00\x00\x10\xa9\x03\x4a\x10\x1c\x08\xe0\x00\x00\x55\x55\x00\x0e\xaa\x2a\x00\x0e\x20\x4e\x00\x00\x08\x88\x01\x38\x08\x80\x08\x88\x00\x28\xf9\xd1\x0c\x48\x13\x20\x13\x20\x00\x06\x04\x0c\xe0\x20\x00\x05\x62\x20\x62\x20\x00\x06\x00\x0e\x04\x43\x07\x49\xaa\x20\x00\x00\x07\x4a\x55\x20\x00\x00\xf0\x20\x00\x00\x00\x00",
  "\x0e\x21\x09\x06\xff\x24\x80\x22\x13\x4b\x52\x02\x01\x3a\x8c\x54\xfc\xd1\x00\x00\x00\x00\x00\x00",
  "\xff\x25\x08\x22\x00\x00\x52\x02\x01\x3a\xa5\x54\xfc\xd1\x00\x00\x00\x00\x00\x00\x00\x00",
  "\x00\x00\x0b\x49\x55\x20\x00\x00\xa0\x20\x00\x00",
  "\x00\x00\x09\x4b\x55\x22\x00\x00\xa0\x22\x00\x00",
};

unsigned int flash_data_a_len[40] = {
  16,
  24,
  188,
  10,
  10,
  16,
  16,
  40,
  40,
  16,
  42,
  16,
  14,
  17,
  36,
  16,
  36,
  16,
  38,
  16,
  8,
  16,
  4,
  16,
  16,
  42,
  16,
  16,
  16,
  16,
  16,
  16,
  12,
  12,
  30,
  88,
  24,
  22,
  12,
  12,
};
char target_device_40AC60 = 0; // weak


//----- (00401000) --------------------------------------------------------
int get_device_type_401000(void)
{
  return target_device_40AC60 & 0xF0;
}
// 40AC60: using guessed type char target_device_40AC60;

//----- (00401010) --------------------------------------------------------
char __cdecl set_device_type_401010(char a1)
{
  char result; // al@1

  result = a1;
  target_device_40AC60 = a1;
  return result;
}
// 40AC60: using guessed type char target_device_40AC60;

//----- (00401020) --------------------------------------------------------
int print_device_list_401020(void)
{
  return printf(
           "Device :\n"
           " - Normal (default) : %d\n"
           " - F2A : %d\n"
           " - PRO : %d\n"
           " - TURBO : %d\n"
           " - EXTREM : %d\n"
           " - EZ : %d\n"
           " - EZ2 : %d\n"
           " - XG : %d\n"
           " - XROM : %d\n",
           0,
           16,
           32,
           48,
           64,
           80,
           96,
           112,
           128);
}

//----- (00401050) --------------------------------------------------------
void __cdecl get_savetype_401050(unsigned char *filecontents, long filesize, int *backuptype, int *version)
{
  unsigned int i; // edi@1
  int len; // esi@2
  unsigned char *result; // eax@2

  *backuptype = 0;
  i = 0;
  while ( 1 )
  {
    len = strlen(savetype_list_40A040[i]);
    result = find_mem_4011E0(filecontents, filesize, savetype_list_40A040[i], len);
    if ( result )
      break;
    if ( ++i >= 6 )
      return;
  }
  *version = *(result + len + 2) + 10 * (*(result + len + 1) + 10 * *(result + len)) - 5328;
  //result = backuptype;
  *backuptype = backuptype_tab_40A058[i];
  return;
}

//----- (004010E0) --------------------------------------------------------
#if 0
int printf(char *arg0, ...)
{
  va_list va; // [sp+8h] [bp+8h]@1

  va_start(va, arg0);
  return vprintf(arg0, va);
}
#endif
//----- (00401100) --------------------------------------------------------
bool __cdecl write_rom_401100(void *data, size_t size, char *fn_out)
{
  FILE *f; // esi@1
  bool result; // al@2

  f = fopen(fn_out, "wb");
  if ( f )
  {
    fwrite(data, size, 1u, f);
    fclose(f);
    result = 1;
  }
  else
  {
    printf("Prob with the file : %s", fn_out);
    result = 0;
  }
  return result;
}

//----- (00401150) --------------------------------------------------------
bool __cdecl rom_to_mem_401150(unsigned char **a1, long *filesize, char *filename)
{
  bool v3; // bl@1
  FILE *v4; // eax@1
  FILE *v5; // esi@1
  bool result; // al@2
  unsigned char *v7; // eax@4

  v3 = 0;
  v4 = fopen(filename, "rb");
  v5 = v4;
  if ( v4 )
  {
    if ( !fseek(v4, 0, 2) )
    {
      *filesize = ftell(v5);
      fseek(v5, 0, 0);
      v7 = (unsigned char *)malloc(*filesize);
      *a1 = v7;
      v3 = fread(v7, 1u, *filesize, v5) == *filesize;
    }
    fclose(v5);
    result = v3;
  }
  else
  {
    printf("Prob with the file : %s", filename);
    result = 0;
  }
  return result;
}

//----- (004011E0) --------------------------------------------------------
unsigned char *__cdecl find_mem_4011E0(unsigned char *haystack, long haystacklen, const void *needle, int needlelen)
{
  unsigned char *v4; // ebx@1
  long v5; // ebp@2
  unsigned char *result; // eax@3

  v4 = haystack;
  if ( haystack )
  {
    v5 = haystacklen;
    do
    {
      result = (unsigned char *)memchr(v4, *(const unsigned char*)needle, v5 - needlelen);
      if ( result && !memcmp(result, needle, needlelen) )
        break;
      v5 = v4 - result + v5 - 1;
      v4 = result + 1;
    }
    while ( result );
  }
  else
  {
    result = 0;
  }
  return result;
}

//----- (00401230) --------------------------------------------------------
char *__cdecl get_flash_data_a_hunk_401230(int hunkno, unsigned int *hunklen)
{
  *hunklen = flash_data_a_len[hunkno];
  return flash_data_a_tab[hunkno];
}

//----- (00401250) --------------------------------------------------------
long __cdecl find_flash_data_a_hunk_401250(unsigned char *rom, long romlen, int hunkno)
{
  unsigned char *hunkdata; // ecx@1
  long result; // eax@1

  hunkdata = flash_data_a_tab[hunkno];
  result = -1;
  if ( hunkdata )
    result = find_mem_4011E0(rom, romlen, hunkdata, flash_data_a_len[hunkno]) - rom;
  return result;
}

//----- (00401290) --------------------------------------------------------
char __cdecl rom_insert_flash_data_a_hunk_401290(unsigned char *rom, long romlen, long romoff, int hunkno)
{
  char *v4; // esi@1
  char result; // al@3

  v4 = flash_data_a_tab[hunkno];
  if ( romoff > 0 && v4 )
  {
    qmemcpy(&rom[romoff], v4, flash_data_a_len[hunkno]);
    result = 1;
  }
  else
  {
    result = 0;
  }
  return result;
}

//----- (004012D0) --------------------------------------------------------
bool __cdecl flash_data_a_hunk_replace_4012D0(unsigned char *rom, long romlen, int searchhunk, int replacehunk)
{
  unsigned char *v4; // ecx@1
  long hunkoff; // eax@1
  char *v6; // esi@3
  bool result; // al@5

  v4 = flash_data_a_tab[searchhunk];
  hunkoff = -1;
  if ( v4 )
    hunkoff = find_mem_4011E0(rom, romlen, v4, flash_data_a_len[searchhunk]) - rom;
  v6 = flash_data_a_tab[replacehunk];
  if ( hunkoff > 0 && v6 )
  {
    qmemcpy(&rom[hunkoff], v6, flash_data_a_len[replacehunk]);
    result = 1;
  }
  else
  {
    result = 0;
  }
  return result;
}

//----- (00401340) --------------------------------------------------------
/* this function is buggy af:
   it causes a double-free in the original (which doesn't crash on win),
   which then causes the freed data to be written to the output file -
   resulting in an empty file. it's well possible this one doesn't work
   as intended.
   i fixed the double-free and copy back the modified contents to the
   original rom data allocation.

   only the following games of no-intro romset 202111 use this eeprom
   format:

0002 - Super Mario Advance - Super Mario USA + Mario Brothers (Japan).gba
0032 - Tony Hawk's Pro Skater 2 (USA, Europe).gba
0045 - Rayman Advance (USA) (En,Fr,De,Es,It).gba
0050 - Rayman Advance (Europe) (En,Fr,De,Es,It).gba
0058 - Tony Hawk's Pro Skater 2 (France).gba
0052 - Tony Hawk's Pro Skater 2 (Germany).gba
x069 - Rayman Advance (Europe) (En,Fr,De,Es,It) (Beta).gba

*/
bool __cdecl eeprom_type_a_401340(unsigned char *rom, unsigned long romlen)
{
  long hunk0off; // edi@1
  long hunk3off; // ebx@1
  char *hunk1; // esi@3
  char *hunk2; // eax@3
  char *v6; // edi@3
  char *hunk4; // eax@3
  long v8; // edx@6
  unsigned char *v9; // edi@6
  unsigned int v10; // ecx@6
  unsigned char *v11; // edi@6
  char *v12; // esi@6
  unsigned int v13; // edx@6
  unsigned int v14; // ebp@6
  unsigned char *v16; // esi@6
  unsigned char v18; // cl@6
  bool v19; // zf@7
  int16_t v20; // cx@7
  long v21; // esi@9
  char *v23; // [sp+10h] [bp-18h]@3
  unsigned int hunk2len; // [sp+14h] [bp-14h]@3
  unsigned int hunk1len; // [sp+18h] [bp-10h]@3
  char *v27; // [sp+20h] [bp-8h]@3
  unsigned int hunk4len; // [sp+24h] [bp-4h]@3
  unsigned char *roma; // [sp+2Ch] [bp+4h]@6

  hunk0off = find_flash_data_a_hunk_401250(rom, romlen, 0);
  v8 = hunk0off;
  hunk3off = find_flash_data_a_hunk_401250(rom, romlen, 3);
  if ( hunk3off <= 0 || hunk0off <= 0 )
  {
    printf("Motif for version V11x not found");
    return 0;
  }
  hunk1 = get_flash_data_a_hunk_401230(1, &hunk1len);
  hunk2 = get_flash_data_a_hunk_401230(2, &hunk2len);
  v6 = hunk2;
  v23 = hunk2;
  hunk4 = get_flash_data_a_hunk_401230(4, &hunk4len);
  v27 = hunk4;
  if ( !hunk1 || !v6 || !hunk4 )
    return 0;
  v6[184] = hunk3off + 33;
  v6[186] = hunk3off >> 16;
  v9 = &rom[v8];
  LOBYTE(v8) = hunk1len;
  v10 = hunk1len >> 2;
  qmemcpy(v9, hunk1, 4 * (hunk1len >> 2));
  v12 = &hunk1[4 * v10];
  v11 = &v9[4 * v10];
  LOBYTE(v10) = v8;
  v13 = hunk2len;
  qmemcpy(v11, v12, v10 & 3);
  v14 = 256 - (unsigned char)(romlen - 1) + romlen - 1;
/*** added ***/
  newromsize = v13 + v14;
  newrom = roma = (unsigned char *)malloc(newromsize);
  qmemcpy(roma, rom, 4 * (romlen >> 2));
  v16 = &rom[4 * (romlen >> 2)];
  qmemcpy(&roma[4 * (romlen >> 2)], v16, romlen & 3);
  free(rom); // call to free(rom) causes a double free later on
  v18 = roma[v14 - 1];
  if ( v18 == -1 || (v19 = v18 == -51, v20 = hunk3off + 31, v19) )
    HIBYTE(v20) = BYTE1(hunk3off);
  v21 = (unsigned long) v27; // FIXME: casting pointer to long assumes POSIX model sizeof(long) == sizeof(void*)
  v23[185] = HIBYTE(v20);
  *(_BYTE *)(v21 + 5) = BYTE1(v14);
  *(_BYTE *)(v21 + 6) = v14 >> 16;
  qmemcpy(&roma[hunk3off], (const void *)v21, hunk4len);
  qmemcpy(&roma[v14], v23, hunk2len);
  return 1;
}

//----- (004014D0) --------------------------------------------------------
bool __cdecl eeprom_type_b_4014D0(unsigned char *rom, long romlen)
{
  bool v2; // bl@1
  bool v3; // al@1
  bool result; // al@3

  v2 = flash_data_a_hunk_replace_4012D0(rom, romlen, 5, 7);
  v3 = flash_data_a_hunk_replace_4012D0(rom, romlen, 6, 8);
  if ( v2 && v3 )
  {
    result = 1;
  }
  else
  {
    printf("Motif for version V12x not found");
    result = 0;
  }
  return result;
}

//----- (00401520) --------------------------------------------------------
bool __cdecl eeprom_type_c_401520(unsigned char *rom, long romlen)
{
  bool v2; // bl@1
  bool v3; // al@1
  bool result; // al@3

  v2 = flash_data_a_hunk_replace_4012D0(rom, romlen, 27, 7);
  v3 = flash_data_a_hunk_replace_4012D0(rom, romlen, 6, 8);
  if ( v2 && v3 )
  {
    result = 1;
  }
  else
  {
    printf("Motif for version V12x not found");
    result = 0;
  }
  return result;
}

//----- (00401570) --------------------------------------------------------
bool __cdecl eeprom_patch_401570(int version, unsigned char *rom, long romlen)
{
  bool result; // al@2

  switch ( version )
  {
    case 100:
    case 110:
    case 111:
      result = eeprom_type_a_401340(rom, romlen);
      break;
    case 120:
    case 121:
    case 122:
      result = eeprom_type_b_4014D0(rom, romlen);
      break;
    case 124:
      result = eeprom_type_c_401520(rom, romlen);
      break;
    default:
      printf("Library version not known or not found");
      result = 0;
      break;
  }
  return result;
}

//----- (00401600) --------------------------------------------------------
bool __cdecl flash_type_b_401600(unsigned char *a1, long a2)
{
  bool v2; // bl@1

  v2 = flash_data_a_hunk_replace_4012D0(a1, a2, 9, 10);
  flash_data_a_hunk_replace_4012D0(a1, a2, 11, 12);
  flash_data_a_hunk_replace_4012D0(a1, a2, 13, 14);
  flash_data_a_hunk_replace_4012D0(a1, a2, 15, 16);
  if ( !v2 )
    printf("Motif for version V12x not found");
  return v2;
}

//----- (00401660) --------------------------------------------------------
bool __cdecl flash_type_c_401660(unsigned char *rom, long romlen)
{
  bool v2; // bl@1

  v2 = flash_data_a_hunk_replace_4012D0(rom, romlen, 17, 18);
  flash_data_a_hunk_replace_4012D0(rom, romlen, 19, 20);
  flash_data_a_hunk_replace_4012D0(rom, romlen, 21, 22);
  flash_data_a_hunk_replace_4012D0(rom, romlen, 23, 22);
  if ( !v2 )
    printf("Motif for version V12x not found");
  return v2;
}

//----- (004016C0) --------------------------------------------------------
bool __cdecl flash_type_d_4016C0(unsigned char *rom, long romlen)
{
  bool v2; // bl@1

  v2 = flash_data_a_hunk_replace_4012D0(rom, romlen, 24, 25);
  flash_data_a_hunk_replace_4012D0(rom, romlen, 26, 20);
  flash_data_a_hunk_replace_4012D0(rom, romlen, 21, 22);
  flash_data_a_hunk_replace_4012D0(rom, romlen, 23, 22);
  if ( !v2 )
    printf("Motif for version V13x not found");
  return v2;
}

//----- (00401720) --------------------------------------------------------
bool __cdecl flash_type_a_401720(unsigned char *rom, long romlen)
{
  bool result; // al@2
  long hunk28off; // ebp@3
  long v4; // eax@5
  bool v5; // bl@5
  bool v6; // al@7
  bool v7; // [sp+5h] [bp-3h]@3
  bool v8; // [sp+6h] [bp-2h]@5
  bool v9; // [sp+7h] [bp-1h]@5

  if ( (unsigned char)get_device_type_401000() == -128 )
  {
    hunk28off = find_flash_data_a_hunk_401250(rom, romlen, 28);
    v7 = flash_data_a_hunk_replace_4012D0(rom, romlen, 28, 34);
    if ( hunk28off > 0 )
    {
      rom[hunk28off + 26] = 0;
      rom[hunk28off + 27] = 0;
    }
    v8 = flash_data_a_hunk_replace_4012D0(rom, romlen, 29, 35);
    v9 = flash_data_a_hunk_replace_4012D0(rom, romlen, 30, 36);
    v4 = find_flash_data_a_hunk_401250(rom, romlen, 31);
    v5 = 0;
    if ( v4 > 0 )
      v5 = rom_insert_flash_data_a_hunk_401290(rom, romlen, v4 - 6, 37);
    flash_data_a_hunk_replace_4012D0(rom, romlen, 32, 38);
    v6 = flash_data_a_hunk_replace_4012D0(rom, romlen, 33, 39);
    if ( v7 && v8 && v9 && v5 && v6 )
    {
      result = 1;
    }
    else
    {
      printf("Motif for version not found");
      result = 0;
    }
  }
  else
  {
    printf("This version is not supported by this device");
    result = 0;
  }
  return result;
}

//----- (00401810) --------------------------------------------------------
bool __cdecl flash_patch_401810(int version, unsigned char *rom, long romlen)
{
  bool result; // al@2

  switch ( version )
  {
    case 102:
    case 103:
      result = flash_type_a_401720(rom, romlen);
      break;
    case 120:
    case 121:
      result = flash_type_b_401600(rom, romlen);
      break;
    case 122:
    case 123:
    case 124:
    case 125:
    case 126:
      result = flash_type_c_401660(rom, romlen);
      break;
    case 130:
    case 131:
      result = flash_type_d_4016C0(rom, romlen);
      break;
    default:
      printf("Library version not known or not found");
      result = 0;
      break;
  }
  return result;
}

//----- (004018C0) --------------------------------------------------------
bool __cdecl patch_rom_4018C0(char *fn_in, char *fn_out)
{
  bool v2; // bl@1
  bool v3; // al@4
  unsigned char *filecontents; // [sp+4h] [bp-10h]@1
  long filesize; // [sp+8h] [bp-Ch]@1
  int version; // [sp+Ch] [bp-8h]@2
  int backuptype; // [sp+10h] [bp-4h]@2

  v2 = 0;
  filesize = 0;
  filecontents = 0;
  if ( rom_to_mem_401150(&filecontents, &filesize, fn_in) )
  {
    get_savetype_401050(filecontents, filesize, &backuptype, &version);
    printf("Backup type: %s, Version: %d\r\n", backuptype_names_40A060[backuptype], version);
    if ( backuptype == 2 )
    {
      if ( (unsigned char)get_device_type_401000() != 80
        && (unsigned char)get_device_type_401000() != 64
        && (unsigned char)get_device_type_401000() != 96
        && (unsigned char)get_device_type_401000() != 112
        && (unsigned char)get_device_type_401000() != -128 )
      {
        printf("This device don't need this rom be patched");
        goto cleanup;
      }
      v3 = flash_patch_401810(version, filecontents, filesize);
    }
    else
    {
      if ( backuptype != 3 )
        goto cleanup;
      v3 = eeprom_patch_401570(version, filecontents, filesize);
    }
    v2 = v3;
    if ( v3 )
      write_rom_401100(newrom ? newrom : filecontents,
		newrom ? newromsize : filesize, fn_out);
  }
cleanup:
  if ( newrom ) filecontents = newrom;
  if ( filecontents )
    free(filecontents);
  return v2;
}

//----- (004019E0) --------------------------------------------------------
int __cdecl main(int argc, char **argv /*, const char **envp*/)
{
  int result; // eax@2
  char v4; // al@3

  if ( argc >= 4 )
  {
    v4 = atol(argv[1]);
    set_device_type_401010(v4);
    result = patch_rom_4018C0((char *)argv[2], (char *)argv[3]) == 0;
  }
  else
  {
    printf(
	"srampatch version : 2.3\n"
	"srampatch device file_in file_out\n"
	"Sample: srampatch 0 truc.gba trucout.gba\n");
    print_device_list_401020();
    result = 1;
  }
  return result;
}

