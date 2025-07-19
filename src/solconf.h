/*
** $Id: solconf.h $
** Configuration file for Sol
** See Copyright Notice in sol.h
*/


#ifndef solconf_h
#define solconf_h

#include <limits.h>
#include <stddef.h>


/*
** ===================================================================
** General Configuration File for Sol
**
** Some definitions here can be changed externally, through the compiler
** (e.g., with '-D' options): They are commented out or protected
** by '#if !defined' guards. However, several other definitions
** should be changed directly here, either because they affect the
** Sol ABI (by making the changes here, you ensure that all software
** connected to Sol, such as C libraries, will be compiled with the same
** configuration); or because they are seldom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) Sol to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ SOL_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want Sol to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define SOL_USE_C89 */


/*
** By default, Sol on Windows use (some) specific Windows features
*/
#if !defined(SOL_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
#define SOL_USE_WINDOWS  /* enable goodies for regular Windows */
#endif


#if defined(SOL_USE_WINDOWS)
#define SOL_DL_DLL	/* enable support for DLL */
#define SOL_USE_C89	/* broadly, Windows is C89 */
#endif


#if defined(SOL_USE_LINUX)
#define SOL_USE_POSIX
#define SOL_USE_DLOPEN		/* needs an extra library: -ldl */
#endif


#if defined(SOL_USE_MACOSX)
#define SOL_USE_POSIX
#define SOL_USE_DLOPEN		/* MacOS does not need -ldl */
#endif


#if defined(SOL_USE_IOS)
#define SOL_USE_POSIX
#define SOL_USE_DLOPEN
#endif


/*
@@ SOLI_IS32INT is true iff 'int' has (at least) 32 bits.
*/
#define SOLI_IS32INT	((UINT_MAX >> 30) >= 3)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types. These options should not be
** set externally, because any other code connected to Sol must
** use the same configuration.
** ===================================================================
*/

/*
@@ SOL_INT_TYPE defines the type for Sol integers.
@@ SOL_FLOAT_TYPE defines the type for Sol floats.
** Sol should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for SOL_INT_TYPE */
#define SOL_INT_INT		1
#define SOL_INT_LONG		2
#define SOL_INT_LONGLONG	3

/* predefined options for SOL_FLOAT_TYPE */
#define SOL_FLOAT_FLOAT		1
#define SOL_FLOAT_DOUBLE	2
#define SOL_FLOAT_LONGDOUBLE	3


/* Default configuration ('long long' and 'double', for 64-bit Sol) */
#define SOL_INT_DEFAULT		SOL_INT_LONGLONG
#define SOL_FLOAT_DEFAULT	SOL_FLOAT_DOUBLE


/*
@@ SOL_32BITS enables Sol with 32-bit integers and 32-bit floats.
*/
#define SOL_32BITS	0


/*
@@ SOL_C89_NUMBERS ensures that Sol uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/
#if defined(SOL_USE_C89) && !defined(SOL_USE_WINDOWS)
#define SOL_C89_NUMBERS		1
#else
#define SOL_C89_NUMBERS		0
#endif


#if SOL_32BITS		/* { */
/*
** 32-bit integers and 'float'
*/
#if SOLI_IS32INT  /* use 'int' if big enough */
#define SOL_INT_TYPE	SOL_INT_INT
#else  /* otherwise use 'long' */
#define SOL_INT_TYPE	SOL_INT_LONG
#endif
#define SOL_FLOAT_TYPE	SOL_FLOAT_FLOAT

#elif SOL_C89_NUMBERS	/* }{ */
/*
** largest types available for C89 ('long' and 'double')
*/
#define SOL_INT_TYPE	SOL_INT_LONG
#define SOL_FLOAT_TYPE	SOL_FLOAT_DOUBLE

#else		/* }{ */
/* use defaults */

#define SOL_INT_TYPE	SOL_INT_DEFAULT
#define SOL_FLOAT_TYPE	SOL_FLOAT_DEFAULT

#endif				/* } */


/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** SOL_PATH_SEP is the character that separates templates in a path.
** SOL_PATH_MARK is the string that marks the substitution points in a
** template.
** SOL_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/
#define SOL_PATH_SEP            ";"
#define SOL_PATH_MARK           "?"
#define SOL_EXEC_DIR            "!"


/*
@@ SOL_PATH_DEFAULT is the default path that Sol uses to look for
** Sol libraries.
@@ SOL_CPATH_DEFAULT is the default path that Sol uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

#define SOL_VDIR	SOL_VERSION_MAJOR "." SOL_VERSION_MINOR
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define SOL_LDIR	"!\\sol\\"
#define SOL_CDIR	"!\\"
#define SOL_SHRDIR	"!\\..\\share\\sol\\" SOL_VDIR "\\"

#if !defined(SOL_PATH_DEFAULT)
#define SOL_PATH_DEFAULT  \
		SOL_LDIR"?.sol;"  SOL_LDIR"?\\init.sol;" \
		SOL_CDIR"?.sol;"  SOL_CDIR"?\\init.sol;" \
		SOL_SHRDIR"?.sol;" SOL_SHRDIR"?\\init.sol;" \
		".\\?.sol;" ".\\?\\init.sol"
#endif

#if !defined(SOL_CPATH_DEFAULT)
#define SOL_CPATH_DEFAULT \
		SOL_CDIR"?.dll;" \
		SOL_CDIR"..\\lib\\sol\\" SOL_VDIR "\\?.dll;" \
		SOL_CDIR"loadall.dll;" ".\\?.dll"
#endif

#else			/* }{ */

#define SOL_ROOT	"/usr/local/"
#define SOL_LDIR	SOL_ROOT "share/sol/" SOL_VDIR "/"
#define SOL_CDIR	SOL_ROOT "lib/sol/" SOL_VDIR "/"

#if !defined(SOL_PATH_DEFAULT)
#define SOL_PATH_DEFAULT  \
		SOL_LDIR"?.sol;"  SOL_LDIR"?/init.sol;" \
		SOL_CDIR"?.sol;"  SOL_CDIR"?/init.sol;" \
		"./?.sol;" "./?/init.sol"
#endif

#if !defined(SOL_CPATH_DEFAULT)
#define SOL_CPATH_DEFAULT \
		SOL_CDIR"?.so;" SOL_CDIR"loadall.so;" "./?.so"
#endif

#endif			/* } */


/*
@@ SOL_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Sol automatically uses "\".)
*/
#if !defined(SOL_DIRSEP)

#if defined(_WIN32)
#define SOL_DIRSEP	"\\"
#else
#define SOL_DIRSEP	"/"
#endif

#endif


/*
** SOL_IGMARK is a mark to ignore all after it when building the
** module name (e.g., used to build the solopen_ function name).
** Typically, the suffix after the mark is the module version,
** as in "mod-v1.2.so".
*/
#define SOL_IGMARK		"-"

/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ SOL_API is a mark for all core API functions.
@@ SOLLIB_API is a mark for all auxiliary library functions.
@@ SOLMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** SOL_BUILD_AS_DLL to get it).
*/
#if defined(SOL_BUILD_AS_DLL)	/* { */

#if defined(SOL_CORE) || defined(SOL_LIB)	/* { */
#define SOL_API __declspec(dllexport)
#else						/* }{ */
#define SOL_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define SOL_API		extern

#endif				/* } */


/*
** More often than not the libs go together with the core.
*/
#define SOLLIB_API	SOL_API
#define SOLMOD_API	SOL_API


/*
@@ SOLI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
@@ SOLI_DDEF and SOLI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (SOLI_DDEF for
** definitions and SOLI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Sol is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define SOLI_FUNC	__attribute__((visibility("internal"))) extern
#else				/* }{ */
#define SOLI_FUNC	extern
#endif				/* } */

#define SOLI_DDEC(dec)	SOLI_FUNC dec
#define SOLI_DDEF	/* empty */

/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ SOL_COMPAT_5_3 controls other macros for compatibility with Sol 5.3.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(SOL_COMPAT_5_3)	/* { */

/*
@@ SOL_COMPAT_MATHLIB controls the presence of several deprecated
** functions in the mathematical library.
** (These functions were already officially removed in 5.3;
** nevertheless they are still available here.)
*/
#define SOL_COMPAT_MATHLIB

/*
@@ SOL_COMPAT_APIINTCASTS controls the presence of macros for
** manipulating other integer types (sol_pushunsigned, sol_tounsigned,
** solL_checkint, solL_checklong, etc.)
** (These macros were also officially removed in 5.3, but they are still
** available here.)
*/
#define SOL_COMPAT_APIINTCASTS


/*
@@ SOL_COMPAT_LT_LE controls the emulation of the '__le' metamethod
** using '__lt'.
*/
#define SOL_COMPAT_LT_LE


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
*/
#define sol_strlen(L,i)		sol_rawlen(L, (i))

#define sol_objlen(L,i)		sol_rawlen(L, (i))

#define sol_equal(L,idx1,idx2)		sol_compare(L,(idx1),(idx2),SOL_OPEQ)
#define sol_lessthan(L,idx1,idx2)	sol_compare(L,(idx1),(idx2),SOL_OPLT)

#endif				/* } */

/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers (low-level part).
** Change these definitions if no predefined SOL_FLOAT_* / SOL_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ SOLI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ SOL_NUMBER_FRMLEN is the length modifier for writing floats.
@@ SOL_NUMBER_FMT is the format for writing floats.
@@ sol_number2str converts a float to a string.
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ sol_str2number converts a decimal numeral to a number.
*/


/* The following definitions are good for most cases here */

#define l_floor(x)		(l_mathop(floor)(x))

#define sol_number2str(s,sz,n)  \
	l_sprintf((s), sz, SOL_NUMBER_FMT, (SOLI_UACNUMBER)(n))

/*
@@ sol_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a sol_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/
#define sol_numbertointeger(n,p) \
  ((n) >= (SOL_NUMBER)(SOL_MININTEGER) && \
   (n) < -(SOL_NUMBER)(SOL_MININTEGER) && \
      (*(p) = (SOL_INTEGER)(n), 1))


/* now the variable definitions */

#if SOL_FLOAT_TYPE == SOL_FLOAT_FLOAT		/* { single float */

#define SOL_NUMBER	float

#define l_floatatt(n)		(FLT_##n)

#define SOLI_UACNUMBER	double

#define SOL_NUMBER_FRMLEN	""
#define SOL_NUMBER_FMT		"%.7g"

#define l_mathop(op)		op##f

#define sol_str2number(s,p)	strtof((s), (p))


#elif SOL_FLOAT_TYPE == SOL_FLOAT_LONGDOUBLE	/* }{ long double */

#define SOL_NUMBER	long double

#define l_floatatt(n)		(LDBL_##n)

#define SOLI_UACNUMBER	long double

#define SOL_NUMBER_FRMLEN	"L"
#define SOL_NUMBER_FMT		"%.19Lg"

#define l_mathop(op)		op##l

#define sol_str2number(s,p)	strtold((s), (p))

#elif SOL_FLOAT_TYPE == SOL_FLOAT_DOUBLE	/* }{ double */

#define SOL_NUMBER	double

#define l_floatatt(n)		(DBL_##n)

#define SOLI_UACNUMBER	double

#define SOL_NUMBER_FRMLEN	""
#define SOL_NUMBER_FMT		"%.14g"

#define l_mathop(op)		op

#define sol_str2number(s,p)	strtod((s), (p))

#else						/* }{ */

#error "numeric float type not defined"

#endif					/* } */



/*
@@ SOL_UNSIGNED is the unsigned version of SOL_INTEGER.
@@ SOLI_UACINT is the result of a 'default argument promotion'
@@ over a SOL_INTEGER.
@@ SOL_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ SOL_INTEGER_FMT is the format for writing integers.
@@ SOL_MAXINTEGER is the maximum value for a SOL_INTEGER.
@@ SOL_MININTEGER is the minimum value for a SOL_INTEGER.
@@ SOL_MAXUNSIGNED is the maximum value for a SOL_UNSIGNED.
@@ sol_integer2str converts an integer to a string.
*/


/* The following definitions are good for most cases here */

#define SOL_INTEGER_FMT		"%" SOL_INTEGER_FRMLEN "d"

#define SOLI_UACINT		SOL_INTEGER

#define sol_integer2str(s,sz,n)  \
	l_sprintf((s), sz, SOL_INTEGER_FMT, (SOLI_UACINT)(n))

/*
** use SOLI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/
#define SOL_UNSIGNED		unsigned SOLI_UACINT


/* now the variable definitions */

#if SOL_INT_TYPE == SOL_INT_INT		/* { int */

#define SOL_INTEGER		int
#define SOL_INTEGER_FRMLEN	""

#define SOL_MAXINTEGER		INT_MAX
#define SOL_MININTEGER		INT_MIN

#define SOL_MAXUNSIGNED		UINT_MAX

#elif SOL_INT_TYPE == SOL_INT_LONG	/* }{ long */

#define SOL_INTEGER		long
#define SOL_INTEGER_FRMLEN	"l"

#define SOL_MAXINTEGER		LONG_MAX
#define SOL_MININTEGER		LONG_MIN

#define SOL_MAXUNSIGNED		ULONG_MAX

#elif SOL_INT_TYPE == SOL_INT_LONGLONG	/* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance */
#if defined(LLONG_MAX)		/* { */
/* use ISO C99 stuff */

#define SOL_INTEGER		long long
#define SOL_INTEGER_FRMLEN	"ll"

#define SOL_MAXINTEGER		LLONG_MAX
#define SOL_MININTEGER		LLONG_MIN

#define SOL_MAXUNSIGNED		ULLONG_MAX

#elif defined(SOL_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types */

#define SOL_INTEGER		__int64
#define SOL_INTEGER_FRMLEN	"I64"

#define SOL_MAXINTEGER		_I64_MAX
#define SOL_MININTEGER		_I64_MIN

#define SOL_MAXUNSIGNED		_UI64_MAX

#else				/* }{ */

#error "Compiler does not support 'long long'. Use option '-DSOL_32BITS' \
  or '-DSOL_C89_NUMBERS' (see file 'solconf.h' for details)"

#endif				/* } */

#else				/* }{ */

#error "numeric integer type not defined"

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Dependencies with C99 and other C details
** ===================================================================
*/

/*
@@ l_sprintf is equivalent to 'snprintf' or 'sprintf' in C89.
** (All uses in Sol have only one format item.)
*/
#if !defined(SOL_USE_C89)
#define l_sprintf(s,sz,f,i)	snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)	((void)(sz), sprintf(s,f,i))
#endif


/*
@@ sol_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'sol_strx2number' undefined and Sol will provide its own
** implementation.
*/
#if !defined(SOL_USE_C89)
#define sol_strx2number(s,p)		sol_str2number(s,p)
#endif


/*
@@ sol_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/
#define sol_pointer2str(buff,sz,p)	l_sprintf(buff,sz,"%p",p)


/*
@@ sol_number2strx converts a float to a hexadecimal numeral.
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** Otherwise, you can leave 'sol_number2strx' undefined and Sol will
** provide its own implementation.
*/
#if !defined(SOL_USE_C89)
#define sol_number2strx(L,b,sz,f,n)  \
	((void)L, l_sprintf(b,sz,f,(SOLI_UACNUMBER)(n)))
#endif


/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a good proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/
#if defined(SOL_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
#undef l_mathop  /* variants not available */
#undef sol_str2number
#define l_mathop(op)		(sol_Number)op  /* no variant */
#define sol_str2number(s,p)	((sol_Number)strtod((s), (p)))
#endif


/*
@@ SOL_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; Sol will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/
#define SOL_KCONTEXT	ptrdiff_t

#if !defined(SOL_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(INTPTR_MAX)  /* even in C99 this type is optional */
#undef SOL_KCONTEXT
#define SOL_KCONTEXT	intptr_t
#endif
#endif


/*
@@ sol_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/
#if !defined(sol_getlocaledecpoint)
#define sol_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif


/*
** macros to improve jump prediction, used mostly for error handling
** and debug facilities. (Some macros in the Sol API use these macros.
** Define SOL_NOBUILTIN if you do not want '__builtin_expect' in your
** code.)
*/
#if !defined(soli_likely)

#if defined(__GNUC__) && !defined(SOL_NOBUILTIN)
#define soli_likely(x)		(__builtin_expect(((x) != 0), 1))
#define soli_unlikely(x)	(__builtin_expect(((x) != 0), 0))
#else
#define soli_likely(x)		(x)
#define soli_unlikely(x)	(x)
#endif

#endif


#if defined(SOL_CORE) || defined(SOL_LIB)
/* shorter names for Sol's own use */
#define l_likely(x)	soli_likely(x)
#define l_unlikely(x)	soli_unlikely(x)
#endif



/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ SOL_NOCVTN2S/SOL_NOCVTS2N control how Sol performs some
** coercions. Define SOL_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define SOL_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define SOL_NOCVTN2S */
/* #define SOL_NOCVTS2N */


/*
@@ SOL_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/
#if defined(SOL_USE_APICHECK)
#include <assert.h>
#define soli_apicheck(l,e)	assert(e)
#endif

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile Sol and when you compile code that links to
** Sol).
** =====================================================================
*/

/*
@@ SOLI_MAXSTACK limits the size of the Sol stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop Sol from consuming unlimited stack
** space (and to reserve some numbers for pseudo-indices).
** (It must fit into max(size_t)/32 and max(int)/2.)
*/
#if SOLI_IS32INT
#define SOLI_MAXSTACK		1000000
#else
#define SOLI_MAXSTACK		15000
#endif


/*
@@ SOL_EXTRASPACE defines the size of a raw memory area associated with
** a Sol state with very fast access.
** CHANGE it if you need a different size.
*/
#define SOL_EXTRASPACE		(sizeof(void *))


/*
@@ SOL_IDSIZE gives the maximum size for the description of the source
** of a function in debug information.
** CHANGE it if you want a different size.
*/
#define SOL_IDSIZE	60


/*
@@ SOLL_BUFFERSIZE is the initial buffer size used by the lauxlib
** buffer system.
*/
#define SOLL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(sol_Number)))


/*
@@ SOLI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define SOLI_MAXALIGN  sol_Number n; double u; void *s; sol_Integer i; long l

/* }================================================================== */





/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/





#endif

