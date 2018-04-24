#pragma once

#include "Utilities/Types.h"

#include <QString>

enum class breakpoint_type
{
	none = 0x0,
	//read = 0x1, // Read only breakpoints aren't supported. So no need to have this value.
	write = 0x2,
	read_write = 0x3,
	exec = 0x4,
};

struct exec_breakpoint_data
{
	//u32 addr; // We don't store address because we map that in the breakpoint handler for efficiency. Maybe change later to vector.
	breakpoint_type type;
	QString name;

	exec_breakpoint_data(breakpoint_type type, const QString& name) : type(type), name(name) {};
	exec_breakpoint_data() : type(breakpoint_type::none), name("breakpoint") {};

	bool operator == (const exec_breakpoint_data& data) { return type == data.type && name == data.name; };
};
