/********************************************************************/
/*    "scan code" <--> "key" map.                                   */
/*    It should be and can only be included by keyboard.c!          */
/********************************************************************/

#ifndef	_ORANGES_KEYMAP_H_
#define	_ORANGES_KEYMAP_H_
#include "ostype.h"
//-----------------------------------------------
// Show character Escape character
//-----------------------------------------------
//Some control character
#define CHAR_INVISIBLE 0
#define KEY_NOT_AVAILABLE 0
/************************************************************************/
/*                          Macros Declaration                          */
/************************************************************************/
#define MAP_COLS	2	/* Number of columns in keymap */
#define NR_SCAN_CODES	0x80	/* Number of scan codes (rows in keymap) */

#define FLAG_BREAK	0x0080          /* Break Code			*/
#define FLAG_EXT	0x00e0          /* Normal function keys		*/
#define MAKE_SHIFT_L	0x002a          /* Shift key			*/
#define MAKE_SHIFT_R	0x0036          /* Shift key			*/
#define MAKE_CTRL_L	0x001d          /* Control key			*/
#define MAKE_CTRL_R	0xe01d          /* Control key			*/
#define MAKE_ALT_L	0x0038          /* Alternate key		*/
#define MAKE_ALT_R	0xe038          /* Alternate key		*/
#define MAKE_CAP_LOCK   0x003a          /* Alternate key		*/
#define MAKE_META_L     0xe05B          /* meta button */
#define MAKE_META_R     0xe05c          /* */

#define BREAK_SHIFT_L	MAKE_SHIFT_L + FLAG_BREAK         /* Shift key			*/
#define BREAK_SHIFT_R	MAKE_SHIFT_R + FLAG_BREAK           /* Shift key			*/
#define BREAK_CTRL_L	MAKE_CTRL_L  + FLAG_BREAK           /* Control key			*/
#define BREAK_CTRL_R	MAKE_CTRL_R  + FLAG_BREAK           /* Control key			*/
#define BREAK_ALT_L	MAKE_ALT_L   + FLAG_BREAK           /* Alternate key		*/
#define BREAK_ALT_R	MAKE_ALT_R   + FLAG_BREAK           /* Alternate key		*/


/* Special keys */
#define ESC             '\033' /*or '\x1b' (0x01 + FLAG_EXT)  Esc */
#define TAB             '\t'   /*(0x0f + FLAG_EXT) TAB          */
#define ENTER           '\r'   /*(0x1c + FLAG_EXT) Enter        */
#define BACKSPACE       '\b'   /*(0x0e + FLAG_EXT) BackSpace    */

/* Shift, Ctrl, Alt */
#define SHIFT_L         CHAR_INVISIBLE /*(0x2a + FLAG_EXT)	 L Shift	*/
#define SHIFT_R         CHAR_INVISIBLE /*(0x36 + FLAG_EXT)	 R Shift	*/
#define CTRL_L          CHAR_INVISIBLE /*(0x1d + FLAG_EXT)	 L Ctrl	*/
#define CTRL_R          CHAR_INVISIBLE /*(0xZZ + FLAG_EXT)	 R Ctrl	e0,1d*/
#define ALT_L           CHAR_INVISIBLE /*(0x38 + FLAG_EXT)	 L Alt	*/
#define ALT_R           CHAR_INVISIBLE /*(0xZZ + FLAG_EXT)	 R Alt	e0,38*/

/* Lock keys */
#define CAPS_LOCK       CHAR_INVISIBLE /*(0x3a + FLAG_EXT)	 Caps Lock	*/

/* Function keys */
#define F1              CHAR_INVISIBLE/*(0x3b + FLAG_EXT)	 F1		*/
#define F2		CHAR_INVISIBLE/*(0x3c + FLAG_EXT)	 F2		*/
#define F3		CHAR_INVISIBLE/*(0x3d + FLAG_EXT)	 F3		*/
#define F4		CHAR_INVISIBLE/*(0x3e + FLAG_EXT)	 F4		*/
#define F5		CHAR_INVISIBLE/*(0x3f + FLAG_EXT)	 F5		*/
#define F6		CHAR_INVISIBLE/*(0x40 + FLAG_EXT)	 F6		*/
#define F7		CHAR_INVISIBLE/*(0x41 + FLAG_EXT)	 F7		*/
#define F8		CHAR_INVISIBLE/*(0x42 + FLAG_EXT)	 F8		*/
#define F9		CHAR_INVISIBLE/*(0x43 + FLAG_EXT)	 F9		*/
#define F10		CHAR_INVISIBLE/*(0x44 + FLAG_EXT)	 F10		*/
#define F11		CHAR_INVISIBLE/*(0x57 + FLAG_EXT)	 F11		*/
#define F12		CHAR_INVISIBLE/*(0x58 + FLAG_EXT)	 F12		*/

/* Control Pad */
#define INSERT	 CHAR_INVISIBLE         	/* Insert e0,52	*/
#define DELETE	 CHAR_INVISIBLE         	/* Delete e0,53 */
#define HOME	 CHAR_INVISIBLE         	/* Home	  e0,47	*/
#define END	 CHAR_INVISIBLE         	/* End	  e0,4f	*/
#define PAGEUP	 CHAR_INVISIBLE         	/* Page Up e0,49	*/
#define PAGEDOWN CHAR_INVISIBLE         	/* Page Down e0,51	*/
#define UP	 CHAR_INVISIBLE         	/* Up	 e0,48	*/
#define DOWN	 CHAR_INVISIBLE         	/* Down	 e0,50  */
#define LEFT	 CHAR_INVISIBLE         	/* Left	 e0,46	*/
#define RIGHT	 CHAR_INVISIBLE         	/* Right e0,4d  */


/* Keymap for US MF-2 keyboard. */

uint_32 keymap[][MAP_COLS] = {
/* scan-code			!Shift		Shift	  */
/* ===================================================== */
/* 0x00 - none		*/      {0,		0    },
/* 0x01 - ESC		*/	{ESC,		ESC  },	   
/* 0x02 - '1'		*/	{'1',		'!'  },	   
/* 0x03 - '2'		*/	{'2',		'@'  },	   
/* 0x04 - '3'		*/	{'3',		'#'  },	   
/* 0x05 - '4'		*/	{'4',		'$'  },	   
/* 0x06 - '5'		*/	{'5',		'%'  },	   
/* 0x07 - '6'		*/	{'6',		'^'  },	   
/* 0x08 - '7'		*/	{'7',		'&'  },	   
/* 0x09 - '8'		*/	{'8',		'*'  },	   
/* 0x0A - '9'		*/	{'9',		'('  },	   
/* 0x0B - '0'		*/	{'0',		')'  },	   
/* 0x0C - '-'		*/	{'-',		'_'  },	   
/* 0x0D - '='		*/	{'=',		'+'  },	   
/* 0x0E - BS		*/	{BACKSPACE,	BACKSPACE},
/* 0x0F - TAB		*/	{TAB,		TAB },	   
/* 0x10 - 'q'		*/	{'q',		'Q' },	   
/* 0x11 - 'w'		*/	{'w',		'W' },	   
/* 0x12 - 'e'		*/	{'e',		'E' },	   
/* 0x13 - 'r'		*/	{'r',		'R' },	   
/* 0x14 - 't'		*/	{'t',		'T' },	   
/* 0x15 - 'y'		*/	{'y',		'Y' },	   
/* 0x16 - 'u'		*/	{'u',		'U' },	   
/* 0x17 - 'i'		*/	{'i',		'I' },	   
/* 0x18 - 'o'		*/	{'o',		'O' },	   
/* 0x19 - 'p'		*/	{'p',		'P' },	   
/* 0x1A - '['		*/	{'[',		'{' },	   
/* 0x1B - ']'		*/	{']',		'}' },	   
/* 0x1C - CR/LF		*/	{ENTER,		ENTER },
/* 0x1D - l. Ctrl	*/	{CTRL_L,        CTRL_L},
/* 0x1E - 'a'		*/	{'a',		'A'},	   
/* 0x1F - 's'		*/	{'s',		'S'},	   
/* 0x20 - 'd'		*/	{'d',		'D'},	   
/* 0x21 - 'f'		*/	{'f',		'F'},	   
/* 0x22 - 'g'		*/	{'g',		'G'},	   
/* 0x23 - 'h'		*/	{'h',		'H'},	   
/* 0x24 - 'j'		*/	{'j',		'J'},	   
/* 0x25 - 'k'		*/	{'k',		'K'},	   
/* 0x26 - 'l'		*/	{'l',		'L'},	   
/* 0x27 - ';'		*/	{';',		':'},	   
/* 0x28 - '\''		*/	{'\'',		'"'},	   
/* 0x29 - '`'		*/	{'`',		'~'},	   
/* 0x2A - l. SHIFT	*/	{SHIFT_L,	SHIFT_L},   
/* 0x2B - '\'		*/	{'\\',		'|'},	   
/* 0x2C - 'z'		*/	{'z',		'Z'},	   
/* 0x2D - 'x'		*/	{'x',		'X'},	   
/* 0x2E - 'c'		*/	{'c',		'C'},	   
/* 0x2F - 'v'		*/	{'v',		'V'},	   
/* 0x30 - 'b'		*/	{'b',		'B'},	   
/* 0x31 - 'n'		*/	{'n',		'N'},	   
/* 0x32 - 'm'		*/	{'m',		'M'},	   
/* 0x33 - ','		*/	{',',		'<'},	   
/* 0x34 - '.'		*/	{'.',		'>'},	   
/* 0x35 - '/'		*/	{'/',		'?'},
/* 0x36 - r. SHIFT	*/	{SHIFT_R,	SHIFT_R},   
/* 0x37 - '*'		*/	{'*',		'*'},       
/* 0x38 - ALT		*/	{ALT_L,		ALT_L},
/* 0x39 - ' '		*/	{' ',		' '},	   
/* 0x3A - CapsLock	*/	{CAPS_LOCK,	CAPS_LOCK}, 
/* 0x3B - F1		*/	{F1,		F1},	   
/* 0x3C - F2		*/	{F2,		F2},	   
/* 0x3D - F3		*/	{F3,		F3},	   
/* 0x3E - F4		*/	{F4,		F4},	   
/* 0x3F - F5		*/	{F5,		F5},	   
/* 0x40 - F6		*/	{F6,		F6},	   
/* 0x41 - F7		*/	{F7,		F7},	   
/* 0x42 - F8		*/	{F8,		F8},	   
/* 0x43 - F9		*/	{F9,		F9},	   
/* 0x44 - F10		*/	{F10,		F10},	   
/* 0x45 - NumLock	*/	{0,             0},    //{NUM_LOCK,	NUM_LOCK},  
/* 0x46 - ScrLock	*/	{0,             0},    //{SCROLL_LOCK,	SCROLL_LOCK},
/* 0x47 - Home		*/	{0,             0},    //{PAD_HOME,	'7'},
/* 0x48 - CurUp		*/	{0,             0},    //{PAD_UP,	'8'},
/* 0x49 - PgUp		*/	{0,             0},    //{PAD_PAGEUP,	'9'},
/* 0x4A - '-'		*/	{0,             0},    //{PAD_MINUS,	'-'},
/* 0x4B - Left		*/	{0,             0},    //{PAD_LEFT,	'4'},
/* 0x4C - MID		*/	{0,             0},    //{PAD_MID,	'5'},
/* 0x4D - Right		*/	{0,             0},    //{PAD_RIGHT,	'6'},
/* 0x4E - '+'		*/	{0,             0},    //{PAD_PLUS,	'+'},
/* 0x4F - End		*/	{0,             0},    //{PAD_END,	'1'},
/* 0x50 - Down		*/	{0,             0},    //{PAD_DOWN,	'2'},
/* 0x51 - PgDown	*/	{0,             0},    //{PAD_PAGEDOWN,	'3'},
/* 0x52 - Insert	*/	{0,             0},    //{PAD_INS,	'0'},
/* 0x53 - Delete	*/	{0,             0},    //{PAD_DOT,	'.'},
/* 0x54 - Enter		*/	{0,		0},
/* 0x55 - ???		*/	{0,		0},
/* 0x56 - ???		*/	{0,		0},
/* 0x57 - F11		*/	{F11,		F11},
/* 0x58 - F12		*/	{F12,		F12},
/* 0x59 - ???		*/	{0,		0},
/* 0x5A - ???		*/	{0,		0},
/* 0x5B - ???		*/	{0,		0},
/* 0x5C - ???		*/	{0,		0},
/* 0x5D - ???		*/	{0,		0},
/* 0x5E - ???		*/	{0,		0},	   	
/* 0x5F - ???		*/	{0,		0},	   
/* 0x60 - ???		*/	{0,		0},	   
/* 0x61 - ???		*/	{0,		0},	   	
/* 0x62 - ???		*/	{0,		0},	   	
/* 0x63 - ???		*/	{0,		0},	   	
/* 0x64 - ???		*/	{0,		0},	   	
/* 0x65 - ???		*/	{0,		0},	   	
/* 0x66 - ???		*/	{0,		0},	   	
/* 0x67 - ???		*/	{0,		0},	   	
/* 0x68 - ???		*/	{0,		0},	   	
/* 0x69 - ???		*/	{0,		0},	   	
/* 0x6A - ???		*/	{0,		0},	   	
/* 0x6B - ???		*/	{0,		0},	   	
/* 0x6C - ???		*/	{0,		0},	   	
/* 0x6D - ???		*/	{0,		0},	   	
/* 0x6E - ???		*/	{0,		0},	   	
/* 0x6F - ???		*/	{0,		0},	   	
/* 0x70 - ???		*/	{0,		0},	   	
/* 0x71 - ???		*/	{0,		0},	   	
/* 0x72 - ???		*/	{0,		0},	   	
/* 0x73 - ???		*/	{0,		0},	   	
/* 0x74 - ???		*/	{0,		0},	   	
/* 0x75 - ???		*/	{0,		0},	   	
/* 0x76 - ???		*/	{0,		0},	   	
/* 0x77 - ???		*/	{0,		0},	   	
/* 0x78 - ???		*/	{0,		0},	   	
/* 0x78 - ???		*/	{0,		0},	   	
/* 0x7A - ???		*/	{0,		0},	   	
/* 0x7B - ???		*/	{0,		0},	   	
/* 0x7C - ???		*/	{0,		0},	   	
/* 0x7D - ???		*/	{0,		0},	   	
/* 0x7E - ???		*/	{0,		0},	   
/* 0x7F - ???		*/	{0,		0},	   
};



/*====================================================================================*
				Appendix: Scan code set 1
 *====================================================================================*

KEY	MAKE	BREAK	-----	KEY	MAKE	BREAK	-----	KEY	MAKE	BREAK
--------------------------------------------------------------------------------------
A	1E	9E		9	0A	8A		[	1A	9A
B	30	B0		`	29	89		INSERT	E0,52	E0,D2
C	2E	AE		-	0C	8C		HOME	E0,47	E0,C7
D	20	A0		=	0D	8D		PG UP	E0,49	E0,C9
E	12	92		\	2B	AB		DELETE	E0,53	E0,D3
F	21	A1		BKSP	0E	8E		END	E0,4F	E0,CF
G	22	A2		SPACE	39	B9		PG DN	E0,51	E0,D1
H	23	A3		TAB	0F	8F		U ARROW	E0,48	E0,C8
I	17	97		CAPS	3A	BA		L ARROW	E0,4B	E0,CB
J	24	A4		L SHFT	2A	AA		D ARROW	E0,50	E0,D0
K	25	A5		L CTRL	1D	9D		R ARROW	E0,4D	E0,CD
L	26	A6		L GUI	E0,5B	E0,DB		NUM	45	C5
M	32	B2		L ALT	38	B8		KP /	E0,35	E0,B5
N	31	B1		R SHFT	36	B6		KP *	37	B7
O	18	98		R CTRL	E0,1D	E0,9D		KP -	4A	CA
P	19	99		R GUI	E0,5C	E0,DC		KP +	4E	CE
Q	10	19		R ALT	E0,38	E0,B8		KP EN	E0,1C	E0,9C
R	13	93		APPS	E0,5D	E0,DD		KP .	53	D3
S	1F	9F		ENTER	1C	9C		KP 0	52	D2
T	14	94		ESC	01	81		KP 1	4F	CF
U	16	96		F1	3B	BB		KP 2	50	D0
V	2F	AF		F2	3C	BC		KP 3	51	D1
W	11	91		F3	3D	BD		KP 4	4B	CB
X	2D	AD		F4	3E	BE		KP 5	4C	CC
Y	15	95		F5	3F	BF		KP 6	4D	CD
Z	2C	AC		F6	40	C0		KP 7	47	C7
0	0B	8B		F7	41	C1		KP 8	48	C8
1	02	82		F8	42	C2		KP 9	49	C9
2	03	83		F9	43	C3		]	1B	9B
3	04	84		F10	44	C4		;	27	A7
4	05	85		F11	57	D7		'	28	A8
5	06	86		F12	58	D8		,	33	B3

6	07	87		PRTSCRN	E0,2A	E0,B7		.	34	B4
					E0,37	E0,AA

7	08	88		SCROLL	46	C6		/	35	B5

8	09	89		PAUSE E1,1D,45	-NONE-				
				      E1,9D,C5


-----------------
ACPI Scan Codes:
-------------------------------------------
Key		Make Code	Break Code
-------------------------------------------
Power		E0, 5E		E0, DE
Sleep		E0, 5F		E0, DF
Wake		E0, 63		E0, E3


-------------------------------
Windows Multimedia Scan Codes:
-------------------------------------------
Key		Make Code	Break Code
-------------------------------------------
Next Track	E0, 19		E0, 99
Previous Track	E0, 10		E0, 90
Stop		E0, 24		E0, A4
Play/Pause	E0, 22		E0, A2
Mute		E0, 20		E0, A0
Volume Up	E0, 30		E0, B0
Volume Down	E0, 2E		E0, AE
Media Select	E0, 6D		E0, ED
E-Mail		E0, 6C		E0, EC
Calculator	E0, 21		E0, A1
My Computer	E0, 6B		E0, EB
WWW Search	E0, 65		E0, E5
WWW Home	E0, 32		E0, B2
WWW Back	E0, 6A		E0, EA
WWW Forward	E0, 69		E0, E9
WWW Stop	E0, 68		E0, E8
WWW Refresh	E0, 67		E0, E7
WWW Favorites	E0, 66		E0, E6

*=====================================================================================*/



#endif /* _ORANGES_KEYMAP_H_ */

