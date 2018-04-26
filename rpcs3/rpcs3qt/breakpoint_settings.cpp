#include "breakpoint_settings.h"

#include "Utilities/File.h"

inline std::string sstr(const QString& _in) { return _in.toStdString(); }

breakpoint_settings::breakpoint_settings(QObject* parent) : QObject(parent)
{
	m_bp_settings = YAML::Load(fs::file{ fs::get_config_dir() + bp_file_name, fs::read + fs::create }.to_string());
}

breakpoint_settings::~breakpoint_settings()
{
}

void breakpoint_settings::SetExecBreakpoint(const QString& gameid, u32 addr, const exec_breakpoint_data& bp_data)
{
	YAML::Node bp_node = YAML::Node();
	bp_node["ui_name"] = sstr(bp_data.m_ui_name);
	bp_node["type"] = static_cast<u32>(bp_data.m_type);
	m_bp_settings[sstr(gameid)][ToHexString(addr)] = bp_node;
	SaveSettings();
}

void breakpoint_settings::SetHWBreakpoint(const QString& gameid, const serialized_hw_breakpoint& bp_data)
{
	YAML::Node bp_node = YAML::Node();
	bp_node["ui_name"] = sstr(bp_data.m_ui_name);
	bp_node["type"] = static_cast<u32>(bp_data.m_type);
	bp_node["size"] = static_cast<u32>(bp_data.m_size);
	bp_node["thread_name"] = bp_data.m_thread_name;
	m_bp_settings[sstr(gameid)][ToHexString(bp_data.m_address)] = bp_node;
	SaveSettings();
}

void breakpoint_settings::RemoveBreakpoint(const QString& gameid, u32 addr)
{
	// why the hell does the std library not have hex conversion to string in std::?? but has stoi?
	m_bp_settings[sstr(gameid)].remove(sstr(QString::number(addr, 16)));
}


QMap<u32, exec_breakpoint_data> breakpoint_settings::ReadExecBreakpoints(const QString& game_id)
{
	QMap<u32, exec_breakpoint_data> ret;
	std::string game = m_bp_settings[sstr(game_id)].Scalar();
	for (auto& breakpoint : m_bp_settings[sstr(game_id)])
	{
		u32 type = stoi(breakpoint.second["type"].Scalar(), nullptr, 16);
		if (static_cast<breakpoint_type>(type) != breakpoint_type::exec)
		{ 
			continue;
		}
		u32 addr = stoi(breakpoint.first.Scalar(), nullptr, 16);
		std::string name = breakpoint.second["name"].Scalar();
		exec_breakpoint_data data = { static_cast<breakpoint_type>(type), QString::fromStdString(name) };
		ret[addr] = data;
	}
	return ret;
}

QVector<serialized_hw_breakpoint> breakpoint_settings::ReadHWBreakpoints(const QString& game_id)
{
	QVector<serialized_hw_breakpoint> ret;
	std::string game = m_bp_settings[sstr(game_id)].Scalar();
	for (auto& breakpoint : m_bp_settings[sstr(game_id)])
	{
		breakpoint_type type = static_cast<breakpoint_type>(stoi(breakpoint.second["type"].Scalar(), nullptr, 16));
		if (type == breakpoint_type::exec)
		{
			continue;
		}

		u32 addr = stoi(breakpoint.first.Scalar(), nullptr, 16);
		QString ui_name = QString::fromStdString(breakpoint.second["ui_name"].Scalar());
		std::string thread_name = breakpoint.second["thread_name"].Scalar();
		hw_breakpoint_size size = static_cast<hw_breakpoint_size>(stoi(breakpoint.second["size"].Scalar()));

		serialized_hw_breakpoint data = serialized_hw_breakpoint(ui_name, thread_name, type, size, addr);
		ret.append(data);
	}
	return ret;
}

std::string breakpoint_settings::ToHexString(u32 num)
{
	return sstr(QString::number(num, 16));
}

void breakpoint_settings::SaveSettings()
{
	YAML::Emitter out;
	out << m_bp_settings;

	fs::file bp_settings = fs::file(fs::get_config_dir() + bp_file_name, fs::read + fs::write + fs::create);

	// Save breakpoints
	bp_settings.seek(0);
	bp_settings.trunc(0);
	bp_settings.write(out.c_str(), out.size());
}
