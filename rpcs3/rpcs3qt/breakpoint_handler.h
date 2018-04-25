#pragma once
#include "stdafx.h"
#include "Emu/System.h"
#include "Emu/Cell/PPUThread.h"
#include "Emu/Cell/PPUOpcodes.h"
#include "Utilities/hw_breakpoint/hw_breakpoint_manager.h"

#include "breakpoint_settings.h"

#include <QMap>

/*
* This class acts as a layer between the UI and Emu for breakpoints.
*/
class breakpoint_handler : public QObject
{
	Q_OBJECT
Q_SIGNALS:
	void BreakpointTriggered(const cpu_thread* thrd, u32 addr);
public:
	explicit breakpoint_handler(QObject* parent = nullptr);

	/**
	* Returns true if breakpoint exists at location otherwise returns 0. Obviously considers current game id.
	*/
	bool HasBreakpoint(u32 addr) const;

	/**
	* Adds a breakpoint at specified address. If breakpoint exists at same address, it is replaced.
	* A transient breakpoint is one that does not get saved to disc.  This would happen when breakpoints are used internally, ie. step over
	*/
	bool AddExecBreakpoint(u32 addr, const QString& name, bool transient = false);

	/**
	* Adds a breakpoint at specified address. If breakpoint exists at same address, it is replaced.
	* Transient breakpoints aren't currently needed for memory.  But, a size is.
	*/
	void AddMemoryBreakpoint(u32 addr, hw_breakpoint_type type, hw_breakpoint_size size, const QString& name);

	/*
	* Changes the specified breakpoint's name. 
	*/
	void RenameBreakpoint(u32 addr, const QString& newName);

	/**
	* Removes the breakpoint at address specified game_id.
	*/
	void RemoveBreakpoint(u32 addr);

	/**
	* Gets the current breakpoints.  Namely, m_exec_breakpoints[m_gameid]
	*/
	QMap<u32, exec_breakpoint_data> GetCurrentBreakpoints()
	{
		return m_exec_breakpoints;
	}

	/**
	* Sets gameid so that I don't need to spam that same argument everywhere.
	*/
	void UpdateGameID();

	/*
	* Ugh. For hardware breakpoints, I need access to the thread.  So, pass it in here.
	*/
	void UpdateCPUThread(std::weak_ptr<cpu_thread> cpu);

	/*
	* delete later.  It's just here so I can test the code before making a proper UI. (proper UI is always last :D)
	*/
	void test();
private:
	QMap<u32, exec_breakpoint_data> m_exec_breakpoints; //! Addr --> Breakpoint Data
	QString m_gameid; // ID of current game being used for breakpoints.
	breakpoint_settings m_breakpoint_settings; // Used to read and write settings to disc.
	std::weak_ptr<cpu_thread> cpu; // needed for hardware breakpoints.
};
