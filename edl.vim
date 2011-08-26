" Vim syntax file
" Language:	EDL
" Maintainer:	Lee Hammerton (Savoury SnaX) savoury.snax@googlemail.com
" Last Change:	2011 August 17

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

syntax match	edlComment	"#.*"

syn keyword	edlReserved		DECLARE ALIAS HANDLER STATES STATE IF NEXT PUSH POP INSTRUCTION EXECUTE ROR ROL MAPPING AFFECT AS INTERNAL
syn keyword	edlReserved		FUNCTION CALL INSTANCE NONZERO NOSIGN NOCARRY INVBIT FORCESET FORCERESET OVERFLOW NOOVERFLOW
syn keyword	edlReserved2		ZERO SIGN PARITYODD PARITYEVEN CARRY BIT C_FUNC_EXTERN PIN IN OUT BIDIRECTIONAL ALWAYS CHANGED TRANSITION
syn keyword	edlDebugReserved	DEBUG_TRACE BASE

syntax match 	edlCurlyError	"}"
syntax region	edlBlock	start="{" end="}" contains=ALLBUT,edlCurlyError fold

syntax match	edlIdentifier	"[a-zA-Z_][a-zA-Z_0-9]*"

syntax match	edlString	"\".\{-}\""
syntax match	edlHexNumber	"\$[0-9a-fA-F]\+"
syntax match	edlBinNumber	"%[0-1]\+"
syntax match	edlDecNumber	"\d\+"

hi def link edlReserved 	Statement
hi def link edlReserved2 	Statement
hi def link edlDebugReserved 	Statement
hi def link edlCurlyError	Error
hi def link edlComment		Comment
hi def link edlHexNumber	Number
hi def link edlDecNumber	Number
hi def link edlBinNumber	Number
hi def link edlString		String

let b:current_syntax = "edl"

" vim: ts=8
