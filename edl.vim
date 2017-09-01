" Vim syntax file
" Language:	EDL
" Maintainer:	Lee Hammerton (Savoury SnaX) savoury.snax@googlemail.com
" Last Change:	2017 September 1

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

syntax match	edlComment	"#.*"

syn keyword	edlReserved			DECLARE ALIAS HANDLER STATES STATE IF NEXT PUSH POP INSTRUCTION EXECUTE ROR ROL MAPPING AFFECT AS INTERNAL
syn keyword	edlReserved			FUNCTION CALL INSTANCE NONZERO NOSIGN NOCARRY INVBIT FORCESET FORCERESET OVERFLOW NOOVERFLOW
syn keyword	edlReserved			ZERO SIGN PARITYODD PARITYEVEN CARRY BIT C_FUNC_EXTERN PIN IN OUT BIDIRECTIONAL ALWAYS CHANGED TRANSITION
syn keyword	edlReserved			CONNECT PULL_UP ISOLATION HIGH_IMPEDANCE
syn keyword	edlDebugReserved	DEBUG_TRACE BASE BUS_TAP

syntax match	edlBraces	display "[{}]"

syntax match	edlOperator		"[-+|&~]"
syntax match	edlOperator		"==\|--\|++"
syntax match	edlOperator		"<-\([^>]\)\@=\|<->\|\([^<]\)\@=->"

syntax match	edlIdentifier	"[a-zA-Z_][a-zA-Z_0-9]*"

syntax match	edlDelimiter	"[();@:.]"

syntax match	edlString		"\".\{-}\""
syntax match	edlHexNumber	"\$[0-9a-fA-F]\+"
syntax match	edlBinNumber	"%[0-1]\+"
syntax match	edlDecNumber	"\d\+"

hi def link edlIdentifier		Identifier
hi def link edlDelimiter		Delimiter
hi def link edlBraces			Delimiter
hi def link edlReserved 		Statement
hi def link edlDebugReserved 	Statement
hi def link edlOperator 		Operator
hi def link edlCurlyError		Error
hi def link edlComment			Comment
hi def link edlHexNumber		Number
hi def link edlDecNumber		Number
hi def link edlBinNumber		Number
hi def link edlString			String

let b:current_syntax = "edl"

" vim: ts=4
