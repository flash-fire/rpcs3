#pragma once

#include "hw_breakpoint.h"
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

/* HW breakpoint that can't yet be inserted into the application since its thread isn't created.
 * So, it's stored in this form until it can be promoted to a regular hw_breakpoint. Yeah, it's messy. But, it's cleaner than other ideas I had.
 * So, this is the serialized form, and also what it'd be read back into when deserialized.
*/
struct serialized_hw_breakpoint
{
	QString m_ui_name; //! Using QString since that's the type used in UI.
	std::string m_thread_name; //! Thread name.  ID can't be used since it could change on various runs. This could lead to issues with thread name copies. But, so could ID!
	breakpoint_type m_type; //! Type: RW or W. Can't use hw version because we need to serialize consistently with other breakpoint type.
	hw_breakpoint_size m_size; //! Size of memory that is refered to in breakpoint.
	u64 m_address; //! address that is used.

	serialized_hw_breakpoint(const QString& ui_name, const std::string& thread_name, const breakpoint_type type, const hw_breakpoint_size size, u64 addr) :	
		m_ui_name(ui_name), m_thread_name(thread_name), m_type(type), m_size(size), m_address(addr)
	{
	}

	serialized_hw_breakpoint() : m_ui_name(), m_thread_name(), m_type(breakpoint_type::none), m_size(hw_breakpoint_size::size_8), m_address()
	{
	}
};

/*
* Holds breakpoint point data for exec breakpoints on PPU.  Address is implicitly stored in breakpoint_handler as a map (efficiency!) so isn't included here. 
*/
struct exec_breakpoint_data
{
	breakpoint_type m_type;
	QString m_ui_name;

	exec_breakpoint_data(breakpoint_type type, const QString& name) : m_type(type), m_ui_name(name) {};
	exec_breakpoint_data() : m_type(breakpoint_type::none), m_ui_name("breakpoint") {};

	bool operator == (const exec_breakpoint_data& data) { return m_type == data.m_type && m_ui_name == data.m_ui_name; };
};
