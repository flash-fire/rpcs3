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

void breakpoint_settings::SetBreakpoint(const QString& gameid, u32 addr, const exec_breakpoint_data& bp_data)
{
	YAML::Node bp_node = YAML::Node();
	bp_node["name"] = sstr(bp_data.name);
	bp_node["type"] = static_cast<u32>(bp_data.type);
	m_bp_settings[sstr(gameid)][addr] = bp_node;

	// Save breakpoint immediately.  Set doesn't happen often so this isn't bad speed wise.
	YAML::Emitter out;
	out << m_bp_settings;

	fs::file bp_settings = fs::file(fs::get_config_dir() + bp_file_name, fs::read + fs::write + fs::create);

	// Save breakpoints
	bp_settings.seek(0);
	bp_settings.trunc(0);
	bp_settings.write(out.c_str(), out.size());
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
		u32 addr = stoi(breakpoint.first.Scalar(), nullptr, 16);
		std::string name = breakpoint.second["name"].Scalar();
		u32 type = stoi(breakpoint.second["type"].Scalar(), nullptr, 16);
		exec_breakpoint_data data = { static_cast<breakpoint_type>(type), QString::fromStdString(name) };
		ret[addr] = data;
	}
	return ret;
}
