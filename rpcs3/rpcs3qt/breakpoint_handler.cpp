#include "breakpoint_handler.h"

constexpr auto qstr = QString::fromStdString;

// PPU Thread functions concerning breakpoints.
extern bool ppu_set_breakpoint(u32 addr);
extern bool ppu_remove_breakpoint(u32 addr);

breakpoint_handler::breakpoint_handler(QObject* parent) : QObject(parent), m_exec_breakpoints(),
	m_gameid("default"), m_breakpoint_settings(this), cpu()
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
	return m_exec_breakpoints.value(addr, {}).type != breakpoint_type::none;
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
		m_breakpoint_settings.SetBreakpoint(m_gameid, addr, m_exec_breakpoints[addr]);
	}
	return true;
}

void breakpoint_handler::AddMemoryBreakpoint(u32 addr, hw_breakpoint_type type, hw_breakpoint_size size, const QString& name)
{
	if (HasBreakpoint(addr))
	{
		RemoveBreakpoint(addr);
	}

	auto cpu = this->cpu.lock();
	if (cpu)
	{
		auto native_handle = (thread_handle) cpu->get()->get_native_handle();
		auto handle = hw_breakpoint_manager::set(native_handle, type, size,
			(u64)vm::g_base_addr + addr, nullptr, [this](const cpu_thread* cpu, hw_breakpoint& breakpoint)
		{
			emit BreakpointTriggered(cpu, breakpoint.get_address());
		});
	}
}

void breakpoint_handler::RenameBreakpoint(u32 addr, const QString& newName)
{
	if (HasBreakpoint(addr))
	{
		m_exec_breakpoints[addr].name = newName;
		m_breakpoint_settings.SetBreakpoint(m_gameid, addr, m_exec_breakpoints[addr]);
	}
}

void breakpoint_handler::RemoveBreakpoint(u32 addr)
{
	breakpoint_type type = m_exec_breakpoints.value(addr, {}).type;
	if (type == breakpoint_type::exec)
	{
		ppu_remove_breakpoint(addr);
		m_exec_breakpoints.remove(addr);
		m_breakpoint_settings.RemoveBreakpoint(m_gameid, addr);
	}
	else if (type == breakpoint_type::read_write || type == breakpoint_type::write)
	{
		// handle that remove
	}
}

void breakpoint_handler::test()
{
	/*u32 addr;

	if (Emu.GetTitleID().empty())
		addr = 0x20200000;
	else
		addr = 0x8abe9c; // TOCS 2

	AddMemoryBreakpoint(addr, hw_breakpoint_type::read_write, hw_breakpoint_size::size_4, "test");*/
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
		if (!vm::check_addr(addr))
			continue;

		// todo: add databreakpoints
		ppu_set_breakpoint(addr);
	}
}

void breakpoint_handler::UpdateCPUThread(std::weak_ptr<cpu_thread> cpu)
{
	this->cpu = cpu;
}
