px4_add_module(
	MODULE examples__checkpoint
	MAIN checkpoint
	STACK_MAIN 2000
	SRCS
		checkpoint.cpp
	DEPENDS
		platforms__common
	)
# vim: set noet ft=cmake fenc=utf-8 ff=unix : 
