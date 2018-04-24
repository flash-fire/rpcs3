#pragma once

#include "breakpoint_types.h"

#include "Utilities/types.h"

#include "yaml-cpp/yaml.h"

#include <QVector>
#include <QMap>
#include <QString>
#include <QSettings>

/**
 * This class acts as a medium to write breakpoints to and from settings.  
 * I wanted to have it in a separate file for useability. Hence, separate file from guisettings. guiconfigs/breakpoints.ini
*/
class breakpoint_settings : public QObject
{
	Q_OBJECT
public:
	explicit breakpoint_settings(QObject* parent = nullptr);
	~breakpoint_settings();
	
	/*
	* Sets a breakpoint at the specified location.
	*/
	void SetBreakpoint(const QString& gameid, u32 addr, const exec_breakpoint_data& bp_data);
	
	/*
	* Removes the breakpoint at specifiied location.
	*/
	void RemoveBreakpoint(const QString& gameid, u32 addr);

	/*
	* Reads all breakpoints from ini file. 
	*/
	QMap<u32, exec_breakpoint_data> ReadExecBreakpoints(const QString& game_id);
private:
	const std::string bp_file_name = "/breakpoints.yml";
	YAML::Node m_bp_settings;
};
