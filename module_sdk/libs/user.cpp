#include "user.h"
#include <shared_mutex>
#include <mutex>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>
#include "database.h"
#include "../../rbslib/String.h"
#include "../../rbslib/Storage.h"
#include "../../rbslib/FileIO.h"
#include "../../json/CJsonObject.h"
#include "../../log/logger.h"
#include "../libs/sqlite_cpp.h"
constexpr auto STUDENT_DIR = "Students";
constexpr auto CLASSES_FILE = "Classes.json";
constexpr auto TEACHER_DIR = "Teachers";
constexpr auto SUBJECT_DIR = "Subjects";
constexpr auto DATABASE_FILE_PATH = "user.db";
//以下锁需要同时加锁多个时按照从上到下的顺序加锁防止死锁
std::shared_mutex Global_Student_Mutex;//读写学生信息文件时加锁
std::shared_mutex Global_Teacher_Mutex;//读写教师信息文件时加锁 
std::shared_mutex Global_Classes_Mutex;//读写班级信息文件时加锁
std::shared_mutex Global_Subjects_Mutex;

void Account::AccountManager::CreateStudent(
	std::uint64_t ID,
	const std::string& name,
	const std::string& phone_number, const std::string& email,
	const std::string& student_sex,
	const std::string& enrollment_date,
	const std::string& pass_word,
	const std::string& college,
	const std::string& class_name,
	const std::string& notes, int permission_level)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	db.Exec(fmt::format("INSERT INTO students ('ID','Name','PhoneNumber','Email','Sex','EnrollmentDate','Password','CollegeName','ClassName','Notes','PermissionLevel') VALUES ({},'{}','{}','{}','{}','{}','{}','{}','{}','{}',{});", ID, name, phone_number, email, student_sex, enrollment_date, pass_word, college, class_name, notes, permission_level));
}

void Account::AccountManager::CreateTeacher(
	std::uint64_t ID,
	const std::string& name,
	const std::string& phone_number, const std::string& email,
	const std::string& teacher_sex,
	const std::string& college,
	const std::string& pass_word,
	const std::string& notes, int permission_level)
{
	
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	db.Exec(fmt::format("INSERT INTO teachers('ID','Name','PhoneNumber','Email','Sex','CollegeName','Password','Notes','PermissionLevel') VALUES ({},'{}','{}','{}','{}','{}','{}','{}',{});", ID, name, phone_number, email, teacher_sex, college, pass_word, notes, permission_level));
}

bool Account::AccountManager::IsStudentExist(std::uint64_t id)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT ID FROM students WHERE ID = {};",id));
	if (temp["ID"].size() == 0)
	{
		return false;
	}
	else return true;
}

bool Account::AccountManager::IsTeacherExist(std::uint64_t id)
{
	
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT ID FROM teachers WHERE ID = {};", id));
	if (temp["ID"].size() == 0)
	{
		return false;
	}
	else return true;
}

void Account::AccountManager::DeleteStudent(std::uint64_t student_id)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT ID FROM students WHERE ID = {};", student_id));
	if (temp["ID"].size() == 0)
	{
		throw AccountException("This student not in the student's tables");
	}
	else
	{
		db.Exec(fmt::format("DELETE FROM students_subjects_relation WHERE StudentID = {};", student_id));
		db.Exec(fmt::format("DELETE FROM students WHERE ID = {};",student_id));
	}
}

void Account::AccountManager::DeleteTeacher(std::uint64_t teacher_id)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	db.Exec(fmt::format("DELETE FROM teachers_subjects_relation WHERE TeacherID = {};", teacher_id));
	db.Exec(fmt::format("DELETE FROM students WHERE ID = {};", teacher_id));
}

auto Account::AccountManager::GetStudentInfo(std::uint64_t id)->StudentBasicInfo
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT * FROM students WHERE ID = {};", id));
	auto temp2 = db.Exec(fmt::format("SELECT SubjectID FROM students_subjects_relation WHERE StudentID = {};", id));
	if (temp["ID"].size() == 0)
	{
		throw AccountException("These students not in the students' tables");
	}
	StudentBasicInfo a;
	a.id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["ID"][0]);
	a.name = temp["Name"][0];
	a.phone_number = temp["PhoneNumber"][0];
	a.sex = temp["Sex"][0];
	a.enrollment_date = temp["PermissionLevel"][0];
	a.notes = temp["Notes"][0];
	a.college = temp["CollegeName"][0];
	a.class_name = temp["ClassName"][0];
	a.password = temp["Password"][0];
	a.permission_level = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["PermissionLevel"][0]);
	a.is_enable = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["IsEnable"][0]);
	a.email = temp["Email"][0];
	for (const auto& it : temp2["SubjectID"])
	{
		a.subjects.push_back(RbsLib::String::Convert::StringToNumber<std::uint64_t>(it));
	}
	return a;
}

auto Account::AccountManager::GetTeacherInfo(std::uint64_t id) -> TeacherBasicInfo
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT * FROM teachers WHERE ID = {};", id));
	auto temp2 = db.Exec(fmt::format("SELECT ClassName FROM classes WHERE TeacherID = {};", id));
	auto temp3 = db.Exec(fmt::format("SELECT SubjectID FROM teachers_subjects_relation WHERE TeacherID = {};", id));
	if (temp["ID"].size() == 0)
	{
		throw AccountException("These teachers not in the teachers' tables");
	}
		TeacherBasicInfo a;
		a.id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["ID"][0]);
		a.name = temp["Name"][0];
		a.phone_number = temp["PhoneNumber"][0];
		a.sex = temp["Sex"][0];
		a.notes = temp["Notes"][0];
		a.college = temp["CollegeName"][0];
		a.password = temp["Password"][0];
		a.permission_level = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["PermissionLevel"][0]);
		a.is_enable = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["IsEnable"][0]);
		a.email = temp["Email"][0];
		for (const auto& it : temp2["ClassName"])
		{
			a.classes.push_back(it);
		}
		for (const auto& it : temp3["SubjectID"])
		{
			a.subjects.push_back(RbsLib::String::Convert::StringToNumber<std::uint64_t>(it));
		}
		return a;
}

void Account::AccountManager::SetStudentProperty(const StudentBasicInfo& info)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT * FROM students WHERE ID = {};", info.id));
	if (temp["ID"].size() == 0)
	{
		throw AccountException("These students not in the students' tables");
	}
	else
	{
		db.Exec(fmt::format("UPDATE students SET Name = '{}' WHERE ID = {};", info.name, info.id));
		db.Exec(fmt::format("UPDATE students SET PhoneNumber = '{}' WHERE ID = {};",info.phone_number , info.id));
		db.Exec(fmt::format("UPDATE students SET Sex = '{}' WHERE ID = {};",info.sex , info.id));
		db.Exec(fmt::format("UPDATE students SET EnrollmentDate = '{}' WHERE ID = {};", info.enrollment_date, info.id));
		db.Exec(fmt::format("UPDATE students SET CollegeName = '{}' WHERE ID = {};", info.college, info.id));
		db.Exec(fmt::format("UPDATE students SET Password = '{}' WHERE ID = {};", info.password, info.id));
		db.Exec(fmt::format("UPDATE students SET PermissionLevel = {} WHERE ID = {};", info.permission_level, info.id));
		db.Exec(fmt::format("UPDATE students SET Notes = '{}' WHERE ID = {};", info.notes, info.id));
		db.Exec(fmt::format("UPDATE students SET IsEnable = {} WHERE ID = {};", info.is_enable, info.id));
		db.Exec(fmt::format("UPDATE students SET Email = '{}' WHERE ID = {};", info.email, info.id));
	}
}

void Account::AccountManager::SetTeacherProperty(const TeacherBasicInfo& info)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT * FROM teachers WHERE ID = {};", info.id));
	if (temp["ID"].size() == 0)
	{
		throw AccountException("These teacher not in the teachers' tables");
	}
	else
	{
		db.Exec(fmt::format("UPDATE teachers SET Name = '{}' WHERE ID = {};", info.name, info.id));
		db.Exec(fmt::format("UPDATE teachers SET PhoneNumber = '{}' WHERE ID = {};", info.phone_number, info.id));
		db.Exec(fmt::format("UPDATE teachers SET Sex = '{}' WHERE ID = {};", info.sex, info.id));
		db.Exec(fmt::format("UPDATE teachers SET CollegeName = '{}' WHERE ID = {};", info.college, info.id));
		db.Exec(fmt::format("UPDATE teachers SET Password = '{}' WHERE ID = {};", info.password, info.id));
		db.Exec(fmt::format("UPDATE teachers SET PermissionLevel = {} WHERE ID = {};", info.permission_level, info.id));
		db.Exec(fmt::format("UPDATE teachers SET Notes = '{}' WHERE ID = {};", info.notes, info.id));
		db.Exec(fmt::format("UPDATE teachers SET IsEnable = {} WHERE ID = {};", info.is_enable, info.id));
		db.Exec(fmt::format("UPDATE teachers SET Email = '{}' WHERE ID = {};", info.email, info.id));
	}
}

auto Account::AccountManager::GetAllStudentInfo(void) -> std::vector<StudentBasicInfo>
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec("SELECT * FROM students;");
	std::vector<StudentBasicInfo> ret;
	std::size_t n = temp["ID"].size();
	for (std::size_t i = 0; i < n; i++)
	{
		StudentBasicInfo a;
		a.id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["ID"][i]);
		a.name = temp["Name"][i];
		a.phone_number = temp["PhoneNumber"][i];
		a.sex = temp["Sex"][i];
		a.enrollment_date = temp["PermissionLevel"][i];
		a.notes = temp["Notes"][i];
		a.college = temp["CollegeName"][i];
		a.class_name = temp["ClassName"][i];
		a.password = temp["Password"][i];
		a.permission_level = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["PermissionLevel"][i]);
		a.is_enable = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["IsEnable"][i]);
		a.email = temp["Email"][i];
		ret.push_back(a);
	}
	return ret;

}

auto Account::AccountManager::GetAllTeacherInfo(void) -> std::vector<TeacherBasicInfo>
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec("SELECT * FROM teachers;");
	
		std::vector<TeacherBasicInfo> ret;
		std::size_t n = temp["ID"].size();
		for (std::size_t i = 0; i < n; i++)
		{
			TeacherBasicInfo a;
			a.id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["ID"][i]);
			a.name = temp["Name"][i];
			a.phone_number = temp["PhoneNumber"][i];
			a.sex = temp["Sex"][i];
			a.notes = temp["Notes"][i];
			a.college = temp["CollegeName"][i];
			a.password = temp["Password"][i];
			a.permission_level = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["PermissionLevel"][i]);
			a.is_enable = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["IsEnable"][i]);
			a.email = temp["Email"][i];
			ret.push_back(a);
		}
		return ret;
	
}

void Account::AccountManager::ChangeStudentClass(std::uint64_t id, const std::string& new_class_name)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT * FROM students WHERE ID = {};", id));
	if (temp["ID"].size() == 0)
	{
		throw AccountException("These students not in the students' tables");
	}
	else
	{
		db.Exec(fmt::format("UPDATE students SET ClassName = '{}' WHERE ID = {};", new_class_name, id));
	}
}
void Account::ClassesManager::CreateClass(const std::string& class_name, std::uint64_t teacherID)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	db.Exec(fmt::format("INSERT INTO classes ('ClassName','TeacherID') VALUES('{}',{});", class_name, teacherID));
}

void Account::ClassesManager::DeleteClass(const std::string& class_name)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT ID FROM students WHERE ClassName = '{}';", class_name));
	if (temp["ClassName"].size() == 0)
	{
		try
		{
			db.Exec(fmt::format("DELETE FROM classes WHERE ClassName = {};", class_name));
		}
		catch (const DataBase::DataBaseException& ex)
		{
			throw AccountException(std::string("Delete class failed : ") + ex.what());
		}
	}
	else
	{
		throw AccountException("This class  in the students' tables");
	}
}

bool Account::ClassesManager::IsClassExist(const std::string& class_name)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT ClassName FROM classes WHERE ClassName = '{}';", class_name));
	return temp["ClassName"].size();
}

auto Account::ClassesManager::GetClassInfo(const std::string& class_name) -> ClassInfo
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT * FROM classes WHERE ClassName = '{}';", class_name));
	auto temp2 = db.Exec(fmt::format("SELECT ID FROM students WHERE ClassName = '{}';", class_name));
	if (temp["ClassName"].size() == 0)
	{
		throw AccountException("This class not in the classes' table");
	}
	ClassInfo a;
	a.create_time = temp["CreateTime"][0];
	a.name = temp["ClassName"][0];
	a.teacher_id = RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["TeacherID"][0]);
	for (const auto& it : temp2["ID"])
	{
		a.students.push_back(RbsLib::String::Convert::StringToNumber<std::uint64_t>(it));
	}
	return a;
}

auto Account::ClassesManager::GetAllClassInfo(void) -> std::vector<ClassInfo>
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	std::vector<ClassInfo> ret;
	auto temp = db.Exec("SELECT * FROM classes;");
	std::size_t classes_number = temp["ClassName"].size();
	for (int i = 0; i < classes_number; ++i)
	{
		ClassInfo c;
		c.name = temp["ClassName"][i];
		c.create_time = temp["CreateTime"][i];
		c.teacher_id = RbsLib::String::Convert::StringToNumber<decltype(c.teacher_id)>(temp["TeacherID"][i]);
		//获取该班级的所有学生
		auto students = db.Exec(fmt::format("SELECT ID FROM students WHERE ClassName='{}';", c.name));
		for (const auto& it : students["ID"]) c.students.push_back(RbsLib::String::Convert::StringToNumber<std::uint64_t>(it));
		ret.push_back(c);
	}
	return ret;
}


void Account::SubjectManager::CreateSubject(std::uint64_t subject_id, const std::string& subject_name, int begin_year, int end_year, int semester, const std::string& classroom, const std::string& description)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp_semester = fmt::format("{}-{}:{}", begin_year, end_year, semester);
	try
	{
		db.Exec(fmt::format("INSERT INTO subjects (ID,Name,Semester,Classroom,Description) VALUES ({},'{}','{}','{}','{}')", subject_id, subject_name, temp_semester, classroom, description));
	}
	catch (const DataBase::DataBaseException& ex)
	{
		throw AccountException(std::string("Create subject failed: ") + ex.what());
	}
}

void Account::SubjectManager::AddStudent(std::uint64_t student_id, std::uint64_t subject_id, const std::string& notes)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	db.Exec(fmt::format(" INSERT INTO students_subjects_relation(StudentID,SubjectID,Grade,Notes) VALUES ({},{},{},'{}');", student_id, subject_id, -1, notes));
}

void Account::SubjectManager::AddTeacher(std::uint64_t teacher_id, std::uint64_t subject_id)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	db.Exec(fmt::format("INSERT INTO teachers_subjects_relation(TeacherID,SubjectID,Notes) VALUES ({},{},'{}');",teacher_id,subject_id,""));
}

void Account::SubjectManager::RemoveStudent(std::uint64_t student_id, std::uint64_t subject_id)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	db.Exec(fmt::format(" DELETE FROM students_subjects_relation WHERE StudentID = {} AND SubjectID = {};", student_id, subject_id));
}

void Account::SubjectManager::RemoveTeacher(std::uint64_t teacher_id, std::uint64_t subject_id)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	db.Exec(fmt::format("DELETE FROM teachers_subjects_relation WHERE TeacherID = {} AND SubjectID = {};", teacher_id, subject_id));
}

void Account::SubjectManager::DeleteSubject(std::uint64_t subject_id)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	db.Exec(fmt::format("DELETE FROM students_subjects_relation WHERE SubjectID = {};",subject_id));
	db.Exec(fmt::format("DELETE FROM teachers_subjects_relation WHERE SubjectID = {};", subject_id));
	db.Exec(fmt::format("DELETE FROM subjects WHERE ID = {};", subject_id));
}

bool Account::SubjectManager::IsSubjectExist(std::uint64_t subject_id)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT ID FROM subjects WHERE ID = {};", subject_id));
	return static_cast<bool>(temp["ID"].size());
}

Account::SubjectInfo Account::SubjectManager::GetSubjectInfo(std::uint64_t subject_id)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto subjects_info = db.Exec(fmt::format("SELECT * FROM subjects WHERE ID={};", subject_id));
	if (subjects_info["ID"].size() == 0) throw AccountException(fmt::format("Subject {} not found", subject_id));
	SubjectInfo f;
	f.id = RbsLib::String::Convert::StringToNumber<decltype(f.id)>(subjects_info["ID"][0]);
	f.name = subjects_info["Name"][0];
	std::sscanf(subjects_info["Semester"][0].c_str(), "%d-%d:%d", &f.semester_start, &f.semester_end, &f.semester);
	f.classroom = subjects_info["ClassRoom"][0];
	f.description = subjects_info["Description"][0];
	//获取课程中的所有学生信息
	auto temp = db.Exec(fmt::format("SELECT * FROM students_subjects_relation WHERE SubjectID={};", f.id));
	std::size_t stu_num = temp["StudentID"].size();
	for (int j = 0; j < stu_num; ++j)
	{
		f.students.push_back(SubjectInfo::Student(
			RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["StudentID"][0]),
			RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["Grade"][0]),
			temp["Notes"][0]));
	}
	//获取课程中所有教师
	temp = db.Exec(fmt::format("SELECT TeacherID FROM teachers_subjects_relation WHERE SubjectID={};", f.id));
	for (const auto& it : temp["TeacherID"]) f.teachers.push_back(RbsLib::String::Convert::StringToNumber<std::uint64_t>(it));
	return f;
}

auto Account::SubjectManager::GetAllSubjectInfo(void) -> std::vector<SubjectInfo>
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	std::vector<SubjectInfo> ret;
	auto subjects_info = db.Exec("SELECT * FROM subjects;");
	std::size_t item_num = subjects_info["ID"].size();
	for (int i = 0; i < item_num; ++i)
	{
		SubjectInfo f;
		f.id = RbsLib::String::Convert::StringToNumber<decltype(f.id)>(subjects_info["ID"][i]);
		f.name = subjects_info["Name"][i];
		std::sscanf(subjects_info["Semester"][i].c_str(), "%d-%d:%d", &f.semester_start, &f.semester_end, &f.semester);
		f.classroom = subjects_info["ClassRoom"][i];
		f.description = subjects_info["Description"][i];
		//获取课程中的所有学生信息
		auto temp = db.Exec(fmt::format("SELECT * FROM students_subjects_relation WHERE SubjectID={};", f.id));
		std::size_t stu_num = temp["StudentID"].size();
		for (int j = 0; j < stu_num; ++j)
		{
			f.students.push_back(SubjectInfo::Student(
				RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["StudentID"][i]),
				RbsLib::String::Convert::StringToNumber<std::uint64_t>(temp["Grade"][i]),
				temp["Notes"][i]));
		}
		//获取课程中所有教师
		temp = db.Exec(fmt::format("SELECT TeacherID FROM teachers_subjects_relation WHERE SubjectID={};", f.id));
		for (const auto& it : temp["TeacherID"]) f.teachers.push_back(RbsLib::String::Convert::StringToNumber<std::uint64_t>(it));
		ret.push_back(f);
	}
	return ret;
}

void Account::SubjectManager::SetStudentProperty(std::uint64_t subject_id, const SubjectInfo::Student& info)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	db.Exec(fmt::format("UPDATE students_subjects_relation SET Grade={},Notes='{}' WHERE StudentID={} AND SubjectID={};", info.grade, info.notes, info.id, subject_id));
}

bool Account::SubjectManager::IsStudentInSubject(std::uint64_t student_id, std::uint64_t subject_id)
{
	auto db = DataBase::SQLite::Open(DATABASE_FILE_PATH);
	auto temp = db.Exec(fmt::format("SELECT * FROM students_subjects_relation WHERE StudentID={} AND SubjectID={};", student_id, subject_id));
	return temp["ID"].size();

}

Account::AccountException::AccountException(const std::string& reason) noexcept
	:reason(reason)
{
}

const char* Account::AccountException::what(void) const noexcept
{
	return this->reason.c_str();
}

Account::SubjectInfo::Student::Student(std::uint64_t id, int grade, const std::string& notes)
	:id(id), grade(grade), notes(notes)
{
}
