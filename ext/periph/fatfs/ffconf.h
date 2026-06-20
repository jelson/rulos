/*---------------------------------------------------------------------------/
/  Configurations of FatFs Module
/---------------------------------------------------------------------------*/

#define FFCONF_DEF	80386	/* Revision ID */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_READONLY	0
/* This option switches read-only configuration. (0:Read/Write or 1:Read-only)
/  Read-only configuration removes writing API functions, f_write(), f_sync(),
/  f_unlink(), f_mkdir(), f_chmod(), f_rename(), f_truncate(), f_getfree()
/  and optional writing functions as well. */


#define FF_FS_MINIMIZE	1
/* This option defines minimization level to remove some basic API functions.
/
/   0: Basic functions are fully enabled.
/   1: f_stat(), f_getfree(), f_unlink(), f_mkdir(), f_truncate() and f_rename()
/      are removed.
/   2: f_opendir(), f_readdir() and f_closedir() are removed in addition to 1.
/   3: f_lseek() function is removed in addition to 2. */


#define FF_USE_FIND		0
/* This option switches filtered directory read functions, f_findfirst() and
/  f_findnext(). (0:Disable, 1:Enable 2:Enable with matching altname[] too) */


#ifndef FF_USE_MKFS
#define FF_USE_MKFS		0
#endif
/* This option switches f_mkfs(). (0:Disable or 1:Enable) */


#define FF_USE_FASTSEEK	0
/* This option switches fast seek feature. (0:Disable or 1:Enable) */


#define FF_USE_EXPAND	0
/* This option switches f_expand(). (0:Disable or 1:Enable) */


#define FF_USE_CHMOD	0
/* This option switches attribute control API functions, f_chmod() and f_utime().
/  (0:Disable or 1:Enable) Also FF_FS_READONLY needs to be 0 to enable this option. */


#define FF_USE_LABEL	0
/* This option switches volume label API functions, f_getlabel() and f_setlabel().
/  (0:Disable or 1:Enable) */


#define FF_USE_FORWARD	0
/* This option switches f_forward(). (0:Disable or 1:Enable) */


#define FF_USE_STRFUNC	0
#define FF_PRINT_LLI	0
#define FF_PRINT_FLOAT	0
#define FF_STRF_ENCODE	3
/* FF_USE_STRFUNC switches string API functions, f_gets(), f_putc(), f_puts() and
/  f_printf().
/
/   0: Disable. FF_PRINT_LLI, FF_PRINT_FLOAT and FF_STRF_ENCODE have no effect.
/   1: Enable without LF-CRLF conversion.
/   2: Enable with LF-CRLF conversion.
*/


/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define FF_CODE_PAGE	437
/* This option specifies the OEM code page to be used on the target system.
/  437 = U.S. (see ffconf.h default comments for other code pages). */


#define FF_USE_LFN		1
#define FF_MAX_LFN		255
/* The FF_USE_LFN switches the support for LFN (long file name).
/
/   0: Disable LFN. FF_MAX_LFN has no effect.
/   1: Enable LFN with static working buffer on the BSS. Always NOT thread-safe.
/   2: Enable LFN with dynamic working buffer on the STACK.
/   3: Enable LFN with dynamic working buffer on the HEAP. */


#define FF_LFN_UNICODE	0
/* 0: ANSI/OEM in current CP (TCHAR = char) */


#define FF_LFN_BUF		255
#define FF_SFN_BUF		12


#define FF_FS_RPATH		0
/* 0: Disable relative path and remove related API functions. */


#define FF_PATH_DEPTH	10
/* exFAT path depth; not used since FF_FS_EXFAT == 0. */


/*---------------------------------------------------------------------------/
/ Drive/Volume Configurations
/---------------------------------------------------------------------------*/

#define FF_VOLUMES		1


#define FF_STR_VOLUME_ID	0
#define FF_VOLUME_STRS		"RAM","NAND","CF","SD","SD2","USB","USB2","USB3"


#define FF_MULTI_PARTITION	0


#define FF_MIN_SS		512
#define FF_MAX_SS		512


#define FF_LBA64		0
/* 64-bit LBA not needed for SD cards in this project. */


#define FF_MIN_GPT		0x10000000


#define FF_USE_TRIM		0


/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_TINY		0


#define FF_FS_EXFAT		0


#define FF_FS_NORTC		1
#define FF_NORTC_MON	1
#define FF_NORTC_MDAY	1
#define FF_NORTC_YEAR	2026
/* No RTC on RULOS targets; all files get a fixed timestamp. */


#define FF_FS_CRTIME	0


#define FF_FS_NOFSINFO	0


#define FF_FS_LOCK		0


#define FF_FS_REENTRANT	0
#define FF_FS_TIMEOUT	1000


/*--- End of configuration options ---*/
