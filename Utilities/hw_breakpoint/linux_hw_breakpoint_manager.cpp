
#include "stdafx.h"

#ifdef __linux__
#include "linux_hw_breakpoint_manager.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/prctl.h>
#include <cstddef>
#include <unistd.h>

std::shared_ptr<hw_breakpoint> linux_hw_breakpoint_manager::set(u32 index, thread_handle thread,
	hw_breakpoint_type type, hw_breakpoint_size size, u64 address, const hw_breakpoint_handler& handler, const void* user_data)
{
	if (!set_impl(index, thread->tid, type, size, address, true))
	{
		return nullptr;
	}

	return std::shared_ptr<hw_breakpoint>(new hw_breakpoint(index, thread, type, size, address, handler, user_data));
}

bool linux_hw_breakpoint_manager::remove(hw_breakpoint& handle)
{
	return set_impl(handle.get_index(), handle.get_thread()->tid, static_cast<hw_breakpoint_type>(0),
		static_cast<hw_breakpoint_size>(0), 0, false);
}

// based on https://github.com/whh8b/hwbp_lib/blob/master/hwbp_lib.c
bool linux_hw_breakpoint_manager::set_impl(u32 index, pid_t parent, hw_breakpoint_type type, hw_breakpoint_size size, u32 address, bool enable)
{
	int child_status = 0;
	pid_t child = fork();

	if (child < 0)
	{
		LOG_FATAL(GENERAL, "%s: fork() failed: %d\n", __func__, errno);
		return false;
	}

	if (child == 0)
	{
		// Child process
		int parent_status = 0;
		if (ptrace(PTRACE_ATTACH, parent, nullptr, nullptr) == -1)
		{
			LOG_FATAL(GENERAL, "%s: child attach failed: %d\n", __func__, errno);
			_exit(1);
		}

		waitpid(parent, &parent_status, 0);

		// Set breakpoint address
		if (!set_debug_register_value(parent, index, address))
		{
			LOG_FATAL(GENERAL, "%s: set breakpoint address failed: %d\n", __func__, errno);
			_exit(1);
		}

		// Get current control register value
		u64 control_register = 0;
		if (!get_debug_register_value(parent, 7, &control_register))
		{
			LOG_FATAL(GENERAL, "%s: get control register value failed: %d\n", __func__, errno);
			_exit(1);
		}

		// Create control register value
		set_debug_control_register_bits(&control_register, index, type, size, true);

		// Set new control register value
		if (!set_debug_register_value(parent, 7, control_register))
		{
			LOG_FATAL(GENERAL, "%s: set control register value failed: %d\n", __func__, errno);
			_exit(1);
		}

		// Detach child from parent
		if (ptrace(PTRACE_DETACH, parent, nullptr, nullptr) == -1)
		{
			LOG_FATAL(GENERAL, "%s: detach failed: %d\n", __func__, errno);
			_exit(1);
		}

		_exit(0);
	}
	else //Parent process
	{
		waitpid(child, &child_status, WUNTRACED);

		if (WIFEXITED(child_status) && !WEXITSTATUS(child_status))
			return true;
	}

	LOG_FATAL(GENERAL, "%s: set breakpoint failed\n", __func__);
	return false;
}

bool linux_hw_breakpoint_manager::get_debug_register_value(pid_t pid, u32 index, u64* value)
{
	return ptrace(PTRACE_PEEKUSER, pid, offsetof(struct user, u_debugreg[index]), value) != -1;
}

bool linux_hw_breakpoint_manager::set_debug_register_value(pid_t pid, u32 index, u64 value)
{
	return ptrace(PTRACE_PEEKUSER, pid, offsetof(struct user, u_debugreg[index]), value) != -1;
}

#endif // #ifdef __linux__
