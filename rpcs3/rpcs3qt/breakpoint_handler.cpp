#include "breakpoint_handler.h"

constexpr auto qstr = QString::fromStdString;

// PPU Thread functions concerning breakpoints.
extern bool ppu_set_breakpoint(u32 addr);
extern bool ppu_remove_breakpoint(u32 addr);

breakpoint_handler::breakpoint_handler(QObject* parent) : QObject(parent), m_exec_breakpoints(),
	m_gameid("default"), m_breakpoint_settings(this)
{
	// Annoying hack since lambdas with captures aren't passed as function pointers--- prevents access violation later if I tried to pass it to ppu_set_breakpoint.
	// I'll probably try and revise this later, but I wanted to get something working for now.
	EmuCallbacks callbacks = Emu.GetCallbacks();
	callbacks.on_ppu_breakpoint_triggered = [this](const ppu_thread& ppu) {
		Q_EMIT BreakpointTriggered(&ppu, ppu.cia);
	};
	Emu.SetCallbacks(std::move(callbacks));
}

bool breakpoint_handler::HasBreakpoint(u32 addr) const
{
	return m_exec_breakpoints.value(addr, {}).m_type != breakpoint_type::none;
}

bool breakpoint_handler::AddExecBreakpoint(u32 addr, const QString& name, bool transient)
{
	if (!vm::check_addr(addr))
	{
		return false;
		Emu.Pause();
	}
	if (HasBreakpoint(addr))
	{
		RemoveBreakpoint(addr);
	}

	if (!ppu_set_breakpoint(addr))
	{
		return false;
	}

	m_exec_breakpoints[addr] = { breakpoint_type::exec, name };
	if (!transient)
	{
		m_breakpoint_settings.SetExecBreakpoint(m_gameid, addr, m_exec_breakpoints[addr]);
	}
	return true;
}

void breakpoint_handler::AddHWBreakpoint(const named_thread* targ_thread, u32 addr, hw_breakpoint_type type, hw_breakpoint_size size, const QString& name)
{
	AddHWBreakpointInternal(targ_thread, addr, GetBreakpointType(type), type, size, name);
}

void breakpoint_handler::RenameBreakpoint(u32 addr, const QString& newName)
{
	if (HasBreakpoint(addr))
	{ 
		//m_exec_breakpoints[addr].m_ui_name = newName; // Technically not needed since the name is inaccessable except when settings are read.
		m_breakpoint_settings.SetExecBreakpoint(m_gameid, addr, m_exec_breakpoints[addr]);
	}
	if (int index = IndexOfHWBreakpoint(addr) != -1)
	{
		// Again, the UI name isn't accessable except through settings so this will work ha. I know it's a hacky workaround but whatever.  Galaxy brain my dudes.
		// As, when the UI loads, it'll use the serialized version, which has the UI name.
		auto breakpoint = m_hw_breakpoints[index];
		// Let's serialize. Ugh
		m_breakpoint_settings.SetHWBreakpoint(m_gameid, serialized_hw_breakpoint(newName,
			*static_cast<const std::string*>(breakpoint->get_user_data()), GetBreakpointType(breakpoint->get_type()), breakpoint->get_size(), breakpoint->get_address()));
	}
}

void breakpoint_handler::RemoveBreakpoint(u32 addr)
{
	breakpoint_type type = m_exec_breakpoints.value(addr, {}).m_type;
	if (type == breakpoint_type::exec)
	{
		ppu_remove_breakpoint(addr);
		m_exec_breakpoints.remove(addr);
		m_breakpoint_settings.RemoveBreakpoint(m_gameid, addr);
	}
	
	if (type == breakpoint_type::read_write || type == breakpoint_type::write)
	{
		u32 index = IndexOfHWBreakpoint(addr);
		if (index == -1)
		{
			return;
		}
		auto targ = m_hw_breakpoints[index];
		m_hw_breakpoints.remove(index);
		hw_breakpoint_manager::remove(*targ);
		m_breakpoint_settings.RemoveBreakpoint(m_gameid, targ->get_address());
	}
}

/*
* Hardware breakpoints are thread specific.  As a result, I need to know when the relevant thread is created.
* IDs can have race conditions.  So, I chose name. But, names have duplicates?  Maybe that's a horrible idea. I'll acquiesce later.
*/
void breakpoint_handler::AddCompatibleHWBreakpoints(const named_thread* thrd)
{
	for (int i = m_ser_hw_breakpoints.size() - 1; i >= 0; --i)
	{
		serialized_hw_breakpoint& breakpoint = m_ser_hw_breakpoints[i];
		if (thrd->get_name() == breakpoint.m_thread_name)
		{
			AddHWBreakpointInternal(thrd, breakpoint.m_address, breakpoint.m_type, GetHWBreakpointType(breakpoint.m_type), breakpoint.m_size, breakpoint.m_ui_name);
			m_ser_hw_breakpoints.remove(i);
		}
	}
}

void breakpoint_handler::test(const named_thread* thrd)
{
	u32 addr;

	if (Emu.GetTitleID().empty())
		addr = 0x20200000;
	else
		addr = 0x20499998; // TOCS 2

	AddHWBreakpoint(thrd, addr, hw_breakpoint_type::read_write, hw_breakpoint_size::size_4, "test");
}

void breakpoint_handler::UpdateGameID()
{
	m_gameid = QString::fromStdString(Emu.GetTitleID());

	// Doubt this will be triggered, but might as well try to use the title if ID somehow doesn't exist?
	if (m_gameid.length() == 0)
	{
		m_gameid = QString::fromStdString(Emu.GetTitle());
	}

	// If you insist...
	if (m_gameid.length() == 0)
	{
		m_gameid = "default";
	}

	// There's a REALLY annoying edge case where step over fails resulting in a breakpoint just sitting transiently that gets caught on next game boot of same game.
	// Gotta reset to old just to prevent that.
	m_exec_breakpoints = m_breakpoint_settings.ReadExecBreakpoints(m_gameid);
	for (u32 addr : m_exec_breakpoints.keys())
	{
		ppu_set_breakpoint(addr);
	}

	// Gotta remove old HW breakpoints.
	for (auto& breakpoint : m_hw_breakpoints)
	{
		hw_breakpoint_manager::remove(*breakpoint);
	}

	m_ser_hw_breakpoints = m_breakpoint_settings.ReadHWBreakpoints(m_gameid);
}

u32 breakpoint_handler::IndexOfHWBreakpoint(u32 addr)
{
	u32 index = -1;
	for (u32 i = 0; i < m_hw_breakpoints.size(); ++i)
	{
		if (m_hw_breakpoints[i]->get_address() == addr)
		{
			index = i;
		}
	}
	return index;
}


void breakpoint_handler::AddHWBreakpointInternal(const named_thread* thread, u32 addr, breakpoint_type settings_type, hw_breakpoint_type type, hw_breakpoint_size size, const QString& name)
{
	if (IndexOfHWBreakpoint(addr) != -1)
	{
		RemoveBreakpoint(addr);
	}
		
	if (thread)
	{
		auto native_handle = (thread_handle)thread->get()->get_native_handle();
		// Delete user data when breakpoint is deleted.  Otherwise, tiny memory leak.  Can't use & in case scope has issues later if thread deletes. 
		// (would happen on game switch I think on game switch since all threads would be deleted when one game ends, another starts)
		std::string* thrd_name = new std::string(thread->get_name());
		auto handle = hw_breakpoint_manager::set(native_handle, type, size,
			(u64)vm::g_base_addr + addr, thrd_name, [this](const cpu_thread* cpu, hw_breakpoint& breakpoint)
		{
			emit BreakpointTriggered(cpu, breakpoint.get_address());
		});
		m_hw_breakpoints.append(handle);
		m_breakpoint_settings.SetHWBreakpoint(m_gameid, { name, thread->get_name(), settings_type, size, addr });
	}
}

hw_breakpoint_type breakpoint_handler::GetHWBreakpointType(breakpoint_type type)
{
	LOG_NOTICE(PPU, "gethwbreakpointtype");
	hw_breakpoint_type hw_type;
	switch (type)
	{
	case breakpoint_type::read_write:
	{
		hw_type = hw_breakpoint_type::read_write;
		break;
	}
	case breakpoint_type::write:
	{
		hw_type = hw_breakpoint_type::write;
		break;
	}
	case breakpoint_type::exec:
	{
		hw_type = hw_breakpoint_type::executable;
		break;
	}
	default:
		hw_type = hw_breakpoint_type::executable;
	}
	return hw_type;
}

breakpoint_type breakpoint_handler::GetBreakpointType(hw_breakpoint_type hw_type)
{
	breakpoint_type type;
	switch (hw_type)
	{
	case hw_breakpoint_type::read_write:
	{
		type = breakpoint_type::read_write;
		break;
	}
	case hw_breakpoint_type::write:
	{
		type = breakpoint_type::write;
		break;
	}
	case hw_breakpoint_type::executable:
	{
		type = breakpoint_type::exec;
		break;
	}
	default:
		type = breakpoint_type::none;
	}
	return type;
}
