#pragma once

#ifdef __linux__

#include "hw_breakpoint_manager_impl.h"

class linux_hw_breakpoint_manager final : public hw_breakpoint_manager_impl
{
protected:
	std::shared_ptr<hw_breakpoint> set(u32 index, thread_handle thread, hw_breakpoint_type type,
		hw_breakpoint_size size, u64 address, const hw_breakpoint_handler& handler, const void* user_data) final;

	bool remove(hw_breakpoint& handle) final;

private:
	bool set_impl(u32 index, pid_t thread, hw_breakpoint_type type, hw_breakpoint_size size, u32 address, bool enable);
	bool get_debug_register_value(pid_t pid, u32 index, u64* value);
	bool set_debug_register_value(pid_t pid, u32 index, u64 value);
};

#endif // #ifdef __linux__
