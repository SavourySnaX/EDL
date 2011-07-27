" Vim syntax file
" Language:	EDL
" Maintainer:	Lee Hammerton (Savoury SnaX) savoury.snax@googlemail.com
" Last Change:	2011 July 21

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

syn keyword	edlReserved		DECLARE ALIAS HANDLER STATES STATE IF
syn keyword	edlDebugReserved	DEBUG_TRACE BASE

syntax match 	edlCurlyError	"}"
syntax region	edlBlock	start="{" end="}" contains=ALLBUT,edlCurlyError fold

syntax match	edlString	"\".\{-}\""
syntax match	edlComment	"#.*"
syntax match	edlHexNumber	"\$[0-9a-fA-F]\+"
syntax match	edlDecNumber	"[0-9]\+"
syntax match	edlBinNumber	"%[0-1]\+"

hi def link edlReserved 	Statement
hi def link edlDebugReserved 	Statement
hi def link edlCurlyError	Error
hi def link edlComment		Comment
hi def link edlHexNumber	Number
hi def link edlDecNumber	Number
hi def link edlBinNumber	Number
hi def link edlString		String

let b:current_syntax = "edl"

" vim: ts=8
