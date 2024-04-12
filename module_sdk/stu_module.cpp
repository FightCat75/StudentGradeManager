#include "module_sdk.h"
#include <iostream>
#include <sstream>
#include <string>
#include "../json/CJsonObject.h"
#include "libs/user.h"
#include "libs/login.h"
#include "../log/logger.h"
#include "libs/generators.h"
#include "../rbslib/String.h"

#include "libs/sqlite_cpp.h"
static void SendError(const RbsLib::Network::TCP::TCPConnection& connection,
	const std::string& message, int status)
{
	try
	{
		neb::CJsonObject robj;
		robj.Add("message", message);
		RbsLib::Network::HTTP::ResponseHeader header;
		header.status = status;
		header.status_descraption = "";
		header.headers.AddHeader("Content-Type", "application/json");
		std::string json = robj.ToFormattedString();
		header.headers.AddHeader("Content-Length", std::to_string(json.length()));
		connection.Send(header.ToBuffer());
		connection.Send(RbsLib::Buffer(json));
	}
	catch (const std::exception& ex)
	{
	}
}
static void SendSuccessResponse(const RbsLib::Network::TCP::TCPConnection& connection, const neb::CJsonObject& obj)
{
	try
	{
		RbsLib::Network::HTTP::ResponseHeader header;
		header.status = 200;
		header.status_descraption = "ok";
		header.headers.AddHeader("Content-Type", "application/json");
		std::string json = obj.ToFormattedString();
		header.headers.AddHeader("Content-Length", std::to_string(json.length()));
		connection.Send(header.ToBuffer());
		connection.Send(RbsLib::Buffer(json));
	}
	catch (const std::exception& ex)
	{
	}
}

static void Login(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	try
	{
		uint64_t id;
		std::stringstream(obj("ID")) >> id;
		auto user = Account::LoginManager::Login(id, obj("Password"));
		Logger::LogInfo("用户[%d:%s]登录成功", user.ID, user.name.c_str());
		neb::CJsonObject robj;
		robj.Add("id", user.ID);
		robj.Add("token", user.token);
		robj.Add("username", user.name);
		robj.Add("permission", user.permission_level);
		robj.Add("message", "ok");
		if (Generator::IsStudentID(id))
			robj.Add("type", "student");
		else if (Generator::IsTeacherID(id))
			robj.Add("type", "teacher");
		RbsLib::Network::HTTP::ResponseHeader header;
		header.status = 200;
		header.status_descraption = "ok";
		header.headers.AddHeader("Content-Type", "application/json");
		std::string json = robj.ToFormattedString();
		header.headers.AddHeader("Content-Length", std::to_string(json.length()));
		request.connection.Send(header.ToBuffer());
		request.connection.Send(RbsLib::Buffer(json));
	}
	catch (const Account::LoginManagerException& ex)
	{
		neb::CJsonObject robj;
		robj.Add("message", ex.what());
		RbsLib::Network::HTTP::ResponseHeader header;
		header.status = 403;
		header.status_descraption = "ok";
		header.headers.AddHeader("Content-Type", "application/json");
		std::string json = robj.ToFormattedString();
		header.headers.AddHeader("Content-Length", std::to_string(json.length()));
		request.connection.Send(header.ToBuffer());
		request.connection.Send(RbsLib::Buffer(json));
	}
	catch (const std::exception& ex)
	{
		neb::CJsonObject robj;
		robj.Add("message", "服务器内部错误");
		RbsLib::Network::HTTP::ResponseHeader header;
		header.status = 503;
		header.status_descraption = "ok";
		header.headers.AddHeader("Content-Type", "application/json");
		std::string json = robj.ToFormattedString();
		header.headers.AddHeader("Content-Length", std::to_string(json.length()));
		request.connection.Send(header.ToBuffer());
		request.connection.Send(RbsLib::Buffer(json));
	}

}

static void Logout(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	std::stringstream(obj("target_id")) >> target_id;
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	obj.Clear();
	//获取目标信息
	if (Generator::IsStudentID(target_id))
	{
		auto target_info = Account::AccountManager::GetStudentInfo(target_id);
		if ((target_info.is_enable == false && basic_info.permission_level == 0) ||
			(target_info.is_enable && (target_info.permission_level > basic_info.permission_level ||
				target_info.id == basic_info.ID)))
		{
			Account::LoginManager::Logout(target_id);
			Logger::LogInfo("用户[%d:%s]退出了[%d:%s]账号", ID, basic_info.name.c_str(),
				target_info.id, target_info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else if (Generator::IsTeacherID(target_id))
	{
		auto target_info = Account::AccountManager::GetTeacherInfo(target_id);
		if ((target_info.is_enable == false && basic_info.permission_level == 0) ||
			(target_info.is_enable == true && target_info.permission_level < basic_info.permission_level ||
				target_id == ID))
		{
			Account::LoginManager::Logout(target_id);
			Logger::LogInfo("用户[%d:%s]退出了[%d:%s]账号", ID, basic_info.name.c_str(),
				target_info.id, target_info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}


static void GetUserInfo(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	std::stringstream(obj("target_id")) >> target_id;
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	obj.Clear();
	//获取目标信息
	if (Generator::IsStudentID(target_id))
	{
		auto target_info = Account::AccountManager::GetStudentInfo(target_id);
		if ((target_info.is_enable == false && basic_info.permission_level == 0) ||
			(target_info.is_enable && (target_info.permission_level > basic_info.permission_level ||
				target_info.id == basic_info.ID || basic_info.permission_level == 0)))
		{
			obj.Add("type", "student");
			obj.Add("name", target_info.name);
			obj.Add("sex", target_info.sex);
			obj.Add("phone_number", target_info.phone_number);
			obj.Add("email", target_info.email);
			obj.Add("enrollment_date", target_info.enrollment_date);
			obj.Add("class_name", target_info.class_name);
			obj.Add("college", target_info.college);
			obj.Add("notes", target_info.notes);
			obj.Add("permission_level", target_info.permission_level);
			Logger::LogInfo("用户[%d:%s]获取了[%d:%s]的个人信息", ID, basic_info.name.c_str(),
				target_info.id, target_info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else if (Generator::IsTeacherID(target_id))
	{
		auto target_info = Account::AccountManager::GetTeacherInfo(target_id);
		if ((target_info.is_enable == false && basic_info.permission_level == 0) ||
			(target_info.is_enable == true && target_info.permission_level < basic_info.permission_level ||
				target_id == ID || basic_info.permission_level == 0))
		{
			obj.Add("type", "Teacher");
			obj.Add("name", target_info.name);
			obj.Add("sex", target_info.sex);
			obj.Add("phone_number", target_info.phone_number);
			obj.Add("email", target_info.email);
			obj.Add("college", target_info.college);
			obj.Add("notes", target_info.notes);
			obj.Add("permission_level", target_info.permission_level);
			Logger::LogInfo("用户[%d:%s]获取了[%d:%s]的个人信息", ID, basic_info.name.c_str(),
				target_info.id, target_info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void GetStudentSubjects(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	std::stringstream(obj("target_id")) >> target_id;
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	obj.Clear();
	//获取目标信息
	if (Generator::IsStudentID(target_id) && Account::AccountManager::IsStudentExist(target_id))
	{
		auto target_info = Account::AccountManager::GetStudentInfo(target_id);
		if ((target_info.is_enable == false && basic_info.permission_level == 0) ||
			(target_info.is_enable && (target_info.permission_level > basic_info.permission_level ||
				target_info.id == basic_info.ID || basic_info.permission_level == 0)))
		{
			obj.AddEmptySubArray("subjects");
			for (auto it : target_info.subjects)
			{
				neb::CJsonObject subobj;
				if (Account::SubjectManager::IsSubjectExist(it))
				{
					auto info = Account::SubjectManager::GetSubjectInfo(it);
					subobj.Add("name", info.name);
					subobj.Add("ID", info.id);
					subobj.Add("classroom", info.classroom);
					subobj.Add("start", info.semester_start);
					subobj.Add("end", info.semester_end);
					subobj.Add("semester", info.semester);
					subobj.Add("description", info.description);
					//查找课程中的该学生
					for (auto& it : info.students)
					{
						if (it.id == target_id)
						{
							subobj.Add("student_note", it.notes);
							subobj.Add("grade", it.grade);
						}
					}
					//获取该课程的老师信息
					subobj.AddEmptySubArray("teachers");
					for (auto it : info.teachers)
					{
						if (Account::AccountManager::IsTeacherExist(it))
						{
							auto tinfo = Account::AccountManager::GetTeacherInfo(it);
							subobj["teachers"].Add(tinfo.name);
						}
					}
				}
				obj["subjects"].Add(subobj);
			}
			Logger::LogInfo("用户[%d:%s]获取了[%d:%s]的课程信息", ID, basic_info.name.c_str(),
				target_info.id, target_info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void GetAllSubjects(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto list = Account::SubjectManager::GetAllSubjectInfo();
	obj.Clear();
	obj.AddEmptySubArray("subjects");
	for (auto& info : list)
	{
		neb::CJsonObject subobj;
		subobj.Add("name", info.name);
		subobj.Add("id", info.id);
		subobj.Add("classroom", info.classroom);
		subobj.Add("start", info.semester_start);
		subobj.Add("end", info.semester_end);
		subobj.Add("semester", info.semester);
		subobj.Add("description", info.description);
		//获取该课程的老师信息
		subobj.AddEmptySubArray("teachers");
		for (auto it : info.teachers)
		{
			if (Account::AccountManager::IsTeacherExist(it))
			{
				auto tinfo = Account::AccountManager::GetTeacherInfo(it);
				subobj["teachers"].Add(tinfo.name);
			}
		}
		obj["subjects"].Add(subobj);
	}
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void ChangeName(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	if (Generator::IsStudentID(target_id) && Account::AccountManager::IsStudentExist(target_id))
	{
		auto info = Account::AccountManager::GetStudentInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			auto old_name = info.name;
			info.name = obj("name");
			if (info.name.empty()) return SendError(request.connection, "名称不可被设置为空", 422);
			Account::AccountManager::SetStudentProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的名称修改为%s", basic_info.ID, basic_info.name.c_str(),
				info.id, old_name.c_str(), info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else if (Generator::IsTeacherID(target_id) && Account::AccountManager::IsTeacherExist(target_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			auto old_name = info.name;
			info.name = obj("name");
			if (info.name.empty()) return SendError(request.connection, "名称不可被设置为空", 422);
			Account::AccountManager::SetTeacherProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的名称修改为%s", basic_info.ID, basic_info.name.c_str(),
				info.id, old_name.c_str(), info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void ChangeSex(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	if (Generator::IsStudentID(target_id) && Account::AccountManager::IsStudentExist(target_id))
	{
		auto info = Account::AccountManager::GetStudentInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			info.sex = obj("sex");
			if (info.sex.empty()) return SendError(request.connection, "性别不可被设置为空", 422);
			Account::AccountManager::SetStudentProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的性别修改为%s", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.sex.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else if (Generator::IsTeacherID(target_id) && Account::AccountManager::IsTeacherExist(target_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			info.sex = obj("sex");
			if (info.sex.empty()) return SendError(request.connection, "性别不可被设置为空", 422);
			Account::AccountManager::SetTeacherProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的性别修改为%s", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.sex.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void ChangePhoneNumber(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	if (Generator::IsStudentID(target_id) && Account::AccountManager::IsStudentExist(target_id))
	{
		auto info = Account::AccountManager::GetStudentInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			info.phone_number = obj("phone_number");
			Account::AccountManager::SetStudentProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的电话号码修改为%s", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.phone_number.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else if (Generator::IsTeacherID(target_id) && Account::AccountManager::IsTeacherExist(target_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			info.phone_number = obj("phone_number");
			Account::AccountManager::SetTeacherProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的电话号码修改为%s", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.phone_number.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void ChangeEnrollmentDate(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	if (Generator::IsStudentID(target_id) && Account::AccountManager::IsStudentExist(target_id))
	{
		auto info = Account::AccountManager::GetStudentInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			info.phone_number = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("enrollment_date"));
			Account::AccountManager::SetStudentProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的入学时间修改为%d", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.enrollment_date);
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void ChangeCollege(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	if (Generator::IsStudentID(target_id) && Account::AccountManager::IsStudentExist(target_id))
	{
		auto info = Account::AccountManager::GetStudentInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			info.college = obj("college");
			Account::AccountManager::SetStudentProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的学院修改为%s", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.college.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else if (Generator::IsTeacherID(target_id) && Account::AccountManager::IsTeacherExist(target_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			info.college = obj("college");
			Account::AccountManager::SetTeacherProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的学院修改为%s", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.college.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void ChangePassword(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	if (Generator::IsStudentID(target_id) && Account::AccountManager::IsStudentExist(target_id))
	{
		auto info = Account::AccountManager::GetStudentInfo(target_id);
		if (basic_info.permission_level == 0 || (info.is_enable == true && info.id == basic_info.ID))
		{
			if (basic_info.permission_level == 0 || info.password == obj("old_password"))
				info.password = obj("password");
			else return SendError(request.connection, "密码错误", 403);
			if (info.password.empty()) return SendError(request.connection, "密码不可被设置为空", 422);
			Account::AccountManager::SetStudentProperty(info);
			Logger::LogInfo("用户[%d:%s]修改了用户[%d:%s]的密码", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else if (Generator::IsTeacherID(target_id) && Account::AccountManager::IsTeacherExist(target_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(target_id);
		if (basic_info.permission_level == 0 || (info.is_enable == true && info.id == basic_info.ID))
		{
			if (basic_info.permission_level == 0 || info.password == obj("old_password"))
				info.password = obj("password");
			else return SendError(request.connection, "密码错误", 403);
			if (info.password.empty()) return SendError(request.connection, "密码不可被设置为空", 422);
			Account::AccountManager::SetTeacherProperty(info);
			Logger::LogInfo("用户[%d:%s]修改了用户[%d:%s]的密码", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else
	{
		return SendError(request.connection, "permission denied", 403);
	}
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void ChangePermissionLevel(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	if (Generator::IsStudentID(target_id) && Account::AccountManager::IsStudentExist(target_id))
	{
		auto info = Account::AccountManager::GetStudentInfo(target_id);
		if (basic_info.permission_level == 0)
		{
			if (obj.KeyExist("permission_level") == false)
			{
				return SendError(request.connection, "未包含permission_level项", 422);
			}
			info.permission_level = RbsLib::String::Convert::StringToNumber<int>(obj("permission_level"));
			if (info.permission_level < 0 || info.permission_level>4) return SendError(request.connection, "permission_level应在0-4之间", 422);
			Account::AccountManager::SetStudentProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的权限等级修改为%d", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.permission_level);
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else if (Generator::IsTeacherID(target_id) && Account::AccountManager::IsTeacherExist(target_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(target_id);
		if (basic_info.permission_level == 0)
		{
			if (obj.KeyExist("permission_level") == false)
			{
				return SendError(request.connection, "未包含permission_level项", 422);
			}
			info.permission_level = RbsLib::String::Convert::StringToNumber<int>(obj("permission_level"));
			if (info.permission_level < 0 || info.permission_level>4) return SendError(request.connection, "permission_level应在0-4之间", 422);
			Account::AccountManager::SetTeacherProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的权限等级修改为%d", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.permission_level);
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void ChangeNotes(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	if (Generator::IsStudentID(target_id) && Account::AccountManager::IsStudentExist(target_id))
	{
		auto info = Account::AccountManager::GetStudentInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			info.notes = obj("notes");
			Account::AccountManager::SetStudentProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的备注修改为\"%s\"", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.notes.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else if (Generator::IsTeacherID(target_id) && Account::AccountManager::IsTeacherExist(target_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(target_id);
		if ((basic_info.permission_level == 0 || info.permission_level > basic_info.permission_level && info.is_enable))
		{
			info.notes = obj("notes");
			Account::AccountManager::SetTeacherProperty(info);
			Logger::LogInfo("用户[%d:%s]将用户[%d:%s]的备注修改为\"%s\"", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str(), info.notes.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void GetTeacherClasses(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	obj.Clear();
	obj.AddEmptySubArray("classes");
	if (Generator::IsTeacherID(target_id) && Account::AccountManager::IsTeacherExist(target_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(target_id);
		if ((basic_info.permission_level == 0 || (target_id == basic_info.ID || info.permission_level > basic_info.permission_level) && info.is_enable))
		{
			for (const auto& it : info.classes)
			{
				obj["classes"].Add(it);
			}
			Logger::LogInfo("用户[%d:%s]获取了教师[%d:%s]所带班级信息", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void GetTeacherSubjects(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
	{
		SendError(request.connection, "invailed token", 403);
		return;
	}
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == 0) return SendError(request.connection, "参数错误", 422);
	obj.Clear();
	obj.AddEmptySubArray("subjects");
	if (Generator::IsTeacherID(target_id) && Account::AccountManager::IsTeacherExist(target_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(target_id);
		if ((basic_info.permission_level == 0 || (target_id == basic_info.ID || info.permission_level > basic_info.permission_level) && info.is_enable))
		{
			for (const auto& it : info.subjects)
			{
				neb::CJsonObject subobj;
				if (Account::SubjectManager::IsSubjectExist(it))
				{
					auto subinfo = Account::SubjectManager::GetSubjectInfo(it);
					subobj.Add("name", subinfo.name);
					subobj.Add("id", subinfo.id);
					obj["subjects"].Add(subobj);
				}
			}
			Logger::LogInfo("用户[%d:%s]获取了教师[%d:%s]所带课程信息", basic_info.ID, basic_info.name.c_str(),
				info.id, info.name.c_str());
		}
		else return SendError(request.connection, "permission denied", 403);
	}
	else return SendError(request.connection, "permission denied", 403);
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void GetClassesInfo(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	if (false == obj.KeyExist("class_name")) return SendError(request.connection, "参数错误", 422);
	auto info = Account::ClassesManager::GetClassInfo(obj("class_name"));
	if (info.teacher_id != basic_info.ID && basic_info.permission_level != 0)
	{
		return SendError(request.connection, "permission denied", 403);
	}
	obj.Clear();
	obj.Add("message", "ok");
	obj.Add("teacher_name", info.name);
	obj.Add("teacher_id", std::to_string(info.teacher_id));
	obj.AddEmptySubArray("students");
	for (auto it : info.students)
	{
		neb::CJsonObject subobj;
		if (Account::AccountManager::IsStudentExist(it))
		{
			subobj.Add("id", it);
			subobj.Add("name", Account::AccountManager::GetStudentInfo(it).name);
			obj["students"].Add(subobj);
		}
	}
	Logger::LogInfo("用户[%d:%s]获取了班级[%s]的信息", basic_info.ID, basic_info.name.c_str(),
		info.name.c_str());
	return SendSuccessResponse(request.connection, obj);
}

static void GetSubjectInfo(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	std::uint64_t subject_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("subject_id"));
	auto subject_info = Account::SubjectManager::GetSubjectInfo(subject_id);
	if (std::find(subject_info.teachers.begin(), subject_info.teachers.end(), basic_info.ID) == subject_info.teachers.end() && basic_info.permission_level != 0)
		return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.Add("message", "ok");
	obj.Add("name", subject_info.name);
	obj.Add("id", subject_info.id);
	obj.Add("classroom", subject_info.classroom);
	obj.Add("start", subject_info.semester_start);
	obj.Add("end", subject_info.semester_end);
	obj.Add("description", subject_info.description);
	obj.AddEmptySubArray("students");
	for (auto& it : subject_info.students)
	{
		neb::CJsonObject subobj;
		if (Account::AccountManager::IsStudentExist(it.id))
		{
			subobj.Add("id", it.id);
			subobj.Add("name", Account::AccountManager::GetStudentInfo(it.id).name);
			subobj.Add("grade", it.grade);
			subobj.Add("notes", it.notes);
			obj["students"].Add(subobj);
		}
	}
	obj.AddEmptySubArray("teachers");
	for (auto& it : subject_info.teachers)
	{
		neb::CJsonObject subobj;
		if (Account::AccountManager::IsTeacherExist(it))
		{
			subobj.Add("id", it);
			subobj.Add("name", Account::AccountManager::GetTeacherInfo(it).name);
			obj["teachers"].Add(subobj);
		}
	}
	Logger::LogInfo("用户[%d:%s]获取了课程[%d:%s]的信息", basic_info.ID, basic_info.name.c_str(),
		subject_info.id, subject_info.name.c_str());
	return SendSuccessResponse(request.connection, obj);
}

static void SetStudentSubjectGrade(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);
	std::uint64_t subject_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("subject_id"));
	if (!Generator::IsSubjectID(subject_id)) return SendError(request.connection, "目标不是课程编号", 422);
	auto subject_info = Account::SubjectManager::GetSubjectInfo(subject_id);
	if (std::find(subject_info.teachers.begin(), subject_info.teachers.end(), basic_info.ID) == subject_info.teachers.end() && basic_info.permission_level != 0)
		return SendError(request.connection, "permission denied", 403);
	std::uint64_t student_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("student_id"));
	if (!Generator::IsStudentID(student_id)) return SendError(request.connection, "目标学生ID不是学号", 422);
	for (auto& it : subject_info.students)
	{
		if (it.id == student_id)
		{
			Account::SubjectInfo::Student stu;
			std::stringstream(obj("grade")) >> stu.grade;
			if (stu.grade > 100 || stu.grade < -1) return SendError(request.connection, "成绩不合法", 422);
			stu.notes = it.notes;
			stu.id = student_id;
			Account::SubjectManager::SetStudentProperty(subject_id, stu);
			obj.Clear();
			obj.Add("message", "ok");
			Logger::LogInfo("用户[%d:%s]设置了课程[%d:%s]中学生[%d]的成绩为%d", basic_info.ID, basic_info.name.c_str(),
				subject_info.id, subject_info.name.c_str(), student_id, stu.grade);
			return SendSuccessResponse(request.connection, obj);
		}
	}
	return SendError(request.connection, "学生不在课程中", 422);
}

static void GetAllStudentsInfo(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.AddEmptySubArray("students");
	auto list = Account::AccountManager::GetAllStudentInfo();
	for (auto& info : list)
	{
		neb::CJsonObject subobj;
		subobj.Add("name", info.name);
		subobj.Add("id", info.id);
		subobj.Add("class", info.class_name);
		subobj.Add("sex", info.sex);
		subobj.Add("notes", info.notes);
		obj["students"].Add(subobj);
	}
	Logger::LogInfo("用户[%d:%s]获取了所有学生信息", basic_info.ID, basic_info.name.c_str());
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void CreateStudent(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查登录信息
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	std::string name = obj("name");
	std::string phone_number = obj("phone_number");
	std::string email = obj("email");
	std::string sex = obj("sex");
	std::string enrollment_date = obj("enrollment_date");
	std::string password = obj("password");
	std::string college = obj("college");
	std::string class_name = obj("class");
	std::string notes = obj("notes");
	int permission_level = RbsLib::String::Convert::StringToNumber<int>(obj("permission_level"));
	//检查名称是否合法
	if (name.empty() || class_name.empty())
		return SendError(request.connection, "姓名或班级不可为空", 422);
	//检查权限等级是否合法
	if (permission_level < 0 || permission_level>4)
		return SendError(request.connection, "权限等级应在0-4之间", 422);
	//检查班级是否存在
	if (Account::ClassesManager::IsClassExist(class_name) == false)
		return SendError(request.connection, "班级不存在", 422);
	//申请学号
	std::uint64_t student_id = Generator::StudentsIDGenerator();
	//创建学生
	try
	{
		Account::AccountManager::CreateStudent(student_id, name, phone_number, email, sex,
			enrollment_date, password, college, class_name, notes, permission_level);
	}
	catch (const std::exception& e)
	{
		Logger::LogError("创建学生[%d]失败：%s", student_id, e.what());
		return SendError(request.connection, "Server error", 500);
	}
	Logger::LogInfo("用户[%d:%s]创建了学生[%d:%s]", basic_info.ID, basic_info.name.c_str(),
		student_id, name.c_str());
	obj.Clear();
	obj.Add("message", "ok");
	obj.Add("id", student_id);
	return SendSuccessResponse(request.connection, obj);
}

static void CreateTeacher(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	//检查登录信息
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	std::string name = obj("name");
	std::string phone_number = obj("phone_number");
	std::string email = obj("email");
	std::string sex = obj("sex");
	std::string password = obj("password");
	std::string college = obj("college");
	std::string notes = obj("notes");
	int permission_level = RbsLib::String::Convert::StringToNumber<int>(obj("permission_level"));
	//检查名称是否合法
	if (name.empty())
		return SendError(request.connection, "姓名不可为空", 422);
	//检查权限等级是否合法
	if (permission_level < 0 || permission_level>4)
		return SendError(request.connection, "权限等级应在0-4之间", 422);
	//申请教师号
	std::uint64_t teacher_id = Generator::TeacherIDGenerator();
	//创建学生
	try
	{
		Account::AccountManager::CreateTeacher(teacher_id, name,
			phone_number, email, sex, college, password, notes, permission_level);
	}
	catch (const std::exception& e)
	{
		Logger::LogError("创建教师[%d]失败：%s", teacher_id, e.what());
		return SendError(request.connection, "Server error", 500);
	}
	Logger::LogInfo("用户[%d:%s]创建了教师[%d:%s]", basic_info.ID, basic_info.name.c_str(),
		teacher_id, name.c_str());
	obj.Clear();
	obj.Add("message", "ok");
	obj.Add("id", teacher_id);
	return SendSuccessResponse(request.connection, obj);
}


static void GetAllTeachersInfo(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.AddEmptySubArray("teachers");
	auto list = Account::AccountManager::GetAllTeacherInfo();
	for (auto& info : list)
	{
		neb::CJsonObject subobj;
		subobj.Add("name", info.name);
		subobj.Add("id", info.id);
		subobj.Add("sex", info.sex);
		subobj.Add("college", info.college);
		subobj.Add("notes", info.notes);
		obj["teachers"].Add(subobj);
	}
	Logger::LogInfo("用户[%d:%s]获取了所有教师信息", basic_info.ID, basic_info.name.c_str());
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void GetAllClasses(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	obj.Clear();
	obj.AddEmptySubArray("classes");
	auto list = Account::ClassesManager::GetAllClassInfo();
	for (const auto& it : list)
	{
		neb::CJsonObject subobj;
		subobj.Add("class_name", it.name);
		subobj.Add("create_time", it.create_time);
		subobj.Add("teacher_id", std::to_string(it.teacher_id));
		subobj.Add("teacher_name", Account::AccountManager::GetTeacherInfo(it.teacher_id).name);
		subobj.Add("students_number", it.students.size());
		obj["classes"].Add(subobj);
	}
	obj.Add("message", "ok");
	Logger::LogInfo("用户[%d:%s]获取了所有班级信息", basic_info.ID, basic_info.name.c_str());
	return SendSuccessResponse(request.connection, obj);
}
static void CreateSubject(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	int start = RbsLib::String::Convert::StringToNumber<int>(obj("start"));
	int end = RbsLib::String::Convert::StringToNumber<int>(obj("end"));
	int semester = RbsLib::String::Convert::StringToNumber<int>(obj("semester"));
	std::string name = obj("name");
	std::string class_room = obj("class_room");
	std::string description = obj("description");
	if (name.empty()) return SendError(request.connection, "课程名称不能为空", 422);
	std::uint64_t subject_id = Generator::SubjectGenerator();
	try
	{

		Account::SubjectManager::CreateSubject(subject_id, name, start, end, semester,
			class_room, description);
	}
	catch (const std::exception& ex)
	{
		Logger::LogError("创建课程失败: %s",ex.what());
	}
	obj.Clear();
	obj.Add("id", subject_id);
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}
static void GetStudentsGradeCSV(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查登录信息
	std::uint64_t ID, subject_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	std::stringstream(obj("subject_id")) >> subject_id;
	//检查用户权限
	//===============================
	std::string csv;
	auto a = Account::SubjectManager::GetSubjectInfo(subject_id);
	for (auto& it : a.students)
	{
		csv += std::to_string(it.id);
		csv += ",";
		csv += Account::AccountManager::GetStudentInfo(it.id).name;
		csv += ",";
		csv += std::to_string(it.grade);
		csv += ",";
		csv += it.notes;
		csv += "\n";
	}
}
static void CreateClass(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	std::string class_name = obj("class_name");
	std::uint64_t teacher_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("teacher_id"));
	if (Account::AccountManager::IsTeacherExist(teacher_id) == false)
		return SendError(request.connection, "教师不存在", 422);
	if (Account::ClassesManager::IsClassExist(class_name))
		return SendError(request.connection, "班级已存在", 422);
	if (class_name.empty())
		return SendError(request.connection, "班级名不可为空", 422);
	try
	{
		Account::ClassesManager::CreateClass(class_name, teacher_id);
	}
	catch (const std::exception& e)
	{
		Logger::LogError("创建班级[%s]失败：%s", class_name.c_str(), e.what());
		return SendError(request.connection, "Server error", 500);
	}
	Logger::LogInfo("用户[%d:%s]创建了班级[%s]", basic_info.ID, basic_info.name.c_str(), class_name.c_str());
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void AddMemberToSubject(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	std::uint64_t member_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("member_id"));
	std::uint64_t subject_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("subject_id"));
	//检查课程是否存在
	if (!Generator::IsSubjectID(subject_id) || !Account::SubjectManager::IsSubjectExist(subject_id))
		return SendError(request.connection, "课程不存在", 422);
	if (Generator::IsStudentID(member_id) && Account::AccountManager::IsStudentExist(member_id))
	{
		//目标成员是学生
		auto info = Account::AccountManager::GetStudentInfo(member_id);
		if (std::find(info.subjects.begin(), info.subjects.end(), subject_id) != info.subjects.end())
			return SendError(request.connection, "该学生已在课程中", 422);
		Account::SubjectManager::AddStudent(member_id, subject_id, obj("notes"));
		Logger::LogInfo("用户[%d:%s]向课程[%d]添加了学生[%d:%s]", basic_info.ID, basic_info.name.c_str(),
			subject_id, member_id, info.name.c_str());
	}
	else if (Generator::IsTeacherID(member_id) && Account::AccountManager::IsTeacherExist(member_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(member_id);
		if (std::find(info.subjects.begin(), info.subjects.end(), subject_id) != info.subjects.end())
			return SendError(request.connection, "该教师已在课程中", 422);
		Account::SubjectManager::AddTeacher(member_id, subject_id);
		Logger::LogInfo("用户[%d:%s]向课程[%d]添加了教师[%d:%s]", basic_info.ID, basic_info.name.c_str(),
			subject_id, member_id, info.name.c_str());
	}
	else return SendError(request.connection, "目标用户不存在", 422);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void RemoveMemberFromSubject(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	std::uint64_t member_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("member_id"));
	std::uint64_t subject_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("subject_id"));
	//检查课程是否存在
	if (!Generator::IsSubjectID(subject_id) || !Account::SubjectManager::IsSubjectExist(subject_id))
		return SendError(request.connection, "课程不存在", 422);
	if (Generator::IsStudentID(member_id) && Account::AccountManager::IsStudentExist(member_id))
	{
		//目标成员是学生
		auto info = Account::AccountManager::GetStudentInfo(member_id);
		if (std::find(info.subjects.begin(), info.subjects.end(), subject_id) == info.subjects.end())
			return SendError(request.connection, "该学生不在课程中", 422);
		Account::SubjectManager::RemoveStudent(member_id, subject_id);
		Logger::LogInfo("用户[%d:%s]从课程[%d]中移除了学生[%d:%s]", basic_info.ID, basic_info.name.c_str(),
			subject_id, member_id, info.name.c_str());

	}
	else if (Generator::IsTeacherID(member_id) && Account::AccountManager::IsTeacherExist(member_id))
	{
		auto info = Account::AccountManager::GetTeacherInfo(member_id);
		if (std::find(info.subjects.begin(), info.subjects.end(), subject_id) == info.subjects.end())
			return SendError(request.connection, "该教师不在课程中", 422);
		Account::SubjectManager::RemoveTeacher(member_id, subject_id);
		Logger::LogInfo("用户[%d:%s]从课程[%d]中移除了教师[%d:%s]", basic_info.ID, basic_info.name.c_str(),
			subject_id, member_id, info.name.c_str());
	}
	else return SendError(request.connection, "目标用户不存在", 422);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void DeleteEmptySubject(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	std::uint64_t subject_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("subject_id"));
	//检查课程是否存在
	if (!Generator::IsSubjectID(subject_id) || !Account::SubjectManager::IsSubjectExist(subject_id))
		return SendError(request.connection, "课程不存在", 422);
	auto info = Account::SubjectManager::GetSubjectInfo(subject_id);
	if (info.students.empty() && info.teachers.empty())
	{
		Account::SubjectManager::DeleteSubject(subject_id);
		Logger::LogInfo("用户[%d:%s]删除了课程[%d:%s]", basic_info.ID, basic_info.name.c_str(),
			subject_id, info.name.c_str());
	}
	else return SendError(request.connection, "课程不为空", 422);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void DeleteEmptyClass(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	std::string class_name = obj("class_name");
	//检查班级是否存在
	if (Account::ClassesManager::IsClassExist(class_name) == false)
		return SendError(request.connection, "班级不存在", 422);
	auto info = Account::ClassesManager::GetClassInfo(class_name);
	//检查班级是否为空
	if (info.students.empty())
	{
		Account::ClassesManager::DeleteClass(class_name);
		Logger::LogInfo("用户[%d:%s]删除了班级[%s]", basic_info.ID, basic_info.name.c_str(), class_name.c_str());
	}
	else return SendError(request.connection, "班级不为空", 422);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}

static void ChangeStudentClass(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	//检查权限
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//检查用户权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);

	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (!Generator::IsStudentID(target_id) || !Account::AccountManager::IsStudentExist(target_id))
		return SendError(request.connection, "目标学生不存在", 403);

	std::string class_name=obj("new_class_name");
	//检查课程是否存在
	if (Account::ClassesManager::IsClassExist(class_name) == false)
	{
		return SendError(request.connection, "班级不存在", 422);
	}
	Account::AccountManager::ChangeStudentClass(target_id, class_name);
	obj.Clear();
	obj.Add("message", "ok");
	Logger::LogInfo("用户[%ld:%s]修改学生[%ld]的班级为%s", ID, basic_info.name.c_str(),
		target_id, class_name.c_str());
	return SendSuccessResponse(request.connection, obj);
}

static void GetStudentsCSV(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查登录信息
	std::uint64_t ID, subject_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	std::stringstream(obj("subject_id")) >> subject_id;
	//检查用户权限
	//===============================
	std::string csv;
	auto a = Account::AccountManager::GetAllStudentInfo();
	for (auto& it : a)
	{
		csv += std::to_string(it.id);
		csv += ",";
		csv += Account::AccountManager::GetStudentInfo(it.id).name;
		csv += ",";
		csv += it.sex;
		csv += ",";
		csv += it.class_name;
		csv += ",";
		csv += it.notes;
		csv += "\n";
	}
}
static void DeleteStudent(const RbsLib::Network::HTTP::Request& request)
{
	neb::CJsonObject obj(request.content.ToString());
	RbsLib::Network::HTTP::ResponseHeader header;
	//检查登录信息
	std::uint64_t ID, target_id = 0;
	std::stringstream(obj("ID")) >> ID;
	if (false == Account::LoginManager::CheckToken(ID, obj("token")))
		return SendError(request.connection, "invailed token", 403);
	auto basic_info = Account::LoginManager::GetOnlineUserInfo(ID);//获取在线用户信息
	//判断权限
	if (basic_info.permission_level != 0) return SendError(request.connection, "permission denied", 403);
	//获取目标学生ID
	target_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(obj("target_id"));
	if (target_id == ID) return SendError(request.connection, "不能删除自己", 422);
	//检查目标学生是否存在
	if (Generator::IsStudentID(target_id) == false || Account::AccountManager::IsStudentExist(target_id) == false)
		return SendError(request.connection, "学生不存在", 422);
	//删除学生
	Account::AccountManager::DeleteStudent(target_id);
	Logger::LogInfo("用户[%d:%s]删除了学生[%ld]", basic_info.ID, basic_info.name.c_str(), target_id);
	obj.Clear();
	obj.Add("message", "ok");
	return SendSuccessResponse(request.connection, obj);
}


//初始化函数，用于模块自身的初始化，主要是描述模块名称版本函数等信息
ModuleSDK::ModuleInfo Init(void)
{
	Logger::LogInfo("正在加载学生成绩管理系统模块");

	//创建模块信息结构体，并分别填入模块名称、版本、描述
	ModuleSDK::ModuleInfo info("stu", "1.0.0", "学生成绩管理模块");

	//为模块添加所需方法
	info.Add("login", Login);
	info.Add("get_user_info", GetUserInfo);
	info.Add("logout", Logout);
	info.Add("get_student_subjects", GetStudentSubjects);
	info.Add("get_all_subjects", GetAllSubjects);
	info.Add("change_name", ChangeName);
	info.Add("change_sex", ChangeSex);
	info.Add("change_phone_number", ChangePhoneNumber);
	info.Add("change_enrollment_date", ChangeEnrollmentDate);
	info.Add("change_college", ChangeCollege);
	info.Add("change_password", ChangePassword);
	info.Add("change_permission_level", ChangePermissionLevel);
	info.Add("change_notes", ChangeNotes);
	info.Add("get_teacher_classes", GetTeacherClasses);
	info.Add("get_class_info", GetClassesInfo);
	info.Add("get_teacher_subjects", GetTeacherSubjects);
	info.Add("get_subject_info", GetSubjectInfo);
	info.Add("set_student_subject_grade", SetStudentSubjectGrade);
	info.Add("get_all_students_info", GetAllStudentsInfo);
	info.Add("create_student", CreateStudent);
	info.Add("get_all_teachers_info", GetAllTeachersInfo);
	info.Add("create_teacher", CreateTeacher);
	info.Add("get_all_classes_info", GetAllClasses);
	info.Add("create_class", CreateClass);
	info.Add("create_subject", CreateSubject);
	info.Add("add_member_to_subject", AddMemberToSubject);
	info.Add("remove_member_from_subject", RemoveMemberFromSubject);
	info.Add("delete_empty_subject", DeleteEmptySubject);
	info.Add("delete_empty_class", DeleteEmptyClass);
	info.Add("change_student_class", ChangeStudentClass);
	info.Add("delete_student", DeleteStudent);

	//初始化模块模块
	auto db = DataBase::SQLite::Open("user.db");
	db.Exec("CREATE TABLE IF NOT EXISTS teachers (ID INTEGER PRIMARY KEY,Name TEXT NOT NULL,PhoneNumber TEXT,Sex TEXT,CollegeName TEXT, Password TEXT, PermissionLevel TINYINT, Notes TEXT, IsEnable TINYINT CHECK(IsEnable IN (0,1)), Email TEXT);");
	db.Exec("CREATE TABLE IF NOT EXISTS classes (ClassName TEXT PRIMARY KEY,TeacherID INTEGER NOT NULL,CreateTime TEXT,FOREIGN KEY(TeacherID) REFERENCES teachers(ID));");
	db.Exec("CREATE TABLE IF NOT EXISTS students (ID INTEGER PRIMARY KEY,Name TEXT NOT NULL,PhoneNumber TEXT,Sex TEXT,EnrollmentDate TEXT, CollegeName TEXT, ClassName TEXT, Password TEXT, PermissionLevel TINYINT, Notes TEXT, IsEnable TINYINT CHECK(IsEnable IN (0,1)), Email TEXT,FOREIGN KEY(ClassName) REFERENCES classes(ClassName));");
	db.Exec("CREATE TABLE IF NOT EXISTS subjects (ID INTEGER PRIMARY KEY,Name TEXT NOT NULL,Semester TEXT,Description TEXT,ClassRoom TEXT);");
	db.Exec("CREATE TABLE IF NOT EXISTS students_subjects_relation (StudentID INTEGER NOT NULL,SubjectID INTEGER NOT NULL,Grade INTEGER,Notes TEXT,FOREIGN KEY(StudentID) REFERENCES students(ID),FOREIGN KEY(SubjectID) REFERENCES subjects(ID),PRIMARY KEY(StudentID,SubjectID));");
	db.Exec("CREATE TABLE IF NOT EXISTS teachers_subjects_relation (TeacherID INTEGER NOT NULL,SubjectID INTEGER NOT NULL,Notes TEXT,PRIMARY KEY(TeacherID,SubjectID),FOREIGN KEY(TeacherID) REFERENCES teachers(ID),FOREIGN KEY(SubjectID) REFERENCES subjects(ID));");
	db.Close();
	// 检查创建默认用户	
	if (Account::AccountManager::GetAllTeacherInfo().empty())
	{
		//没有管理员用户，创建一个
		auto id = Generator::TeacherIDGenerator();
		std::string password = Generator::UUID::GenerateRandom().ToString();
		Account::AccountManager::CreateTeacher(id, "Administrator", "", "", "", "", password, "", 0);
		Logger::LogInfo("已创建默认管理员用户，ID:%d,密码:%s,请及时记录并修改", id, password.c_str());
	}
	//将模块信息返回
	return info;
}
