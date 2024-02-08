#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <exception>
#include <vector>
#include <ctime>
namespace Account
{
	struct StudentBasicInfo
	{
		std::uint64_t id;
		std::string name;
		std::string sex;
		std::string phone_number;
		std::string email;
		int enrollment_date;
		std::string class_name;
		std::string college;
		std::string password;
		int permission_level;
		std::string notes;
		bool is_enable;
		std::vector<std::uint64_t> subjects;
	};
	struct TeacherBasicInfo
	{
		std::uint64_t id;
		std::string name;
		std::string sex;
		std::string phone_number;
		std::string email;
		std::vector<std::string> classes;
		std::vector<std::uint64_t> subjects;
		std::string college;
		std::string password;
		int permission_level;
		std::string notes;
		bool is_enable;
	};
	struct SubjectInfo
	{
		struct Student
		{
			int id;
			int grade;
			std::string notes;
		};
		std::uint64_t id;
		std::string name;
		int semester_start;
		int semester_end;
		std::string classroom;
		std::string description;
		std::vector<std::uint64_t> teachers;
		std::vector <Student> students;
	};
	class AccountException : public std::exception
	{
	private:
		std::string reason;
	public:
		AccountException(const std::string& reason) noexcept;
		const char* what(void) const noexcept override;
	};
	class AccountManager
	{
	public:
		static void CreateStudent(
			std::uint64_t ID,
			const std::string& name,
			const std::string& student_sex, const std::string & email,
			const std::string& phone_number,
			const std::string& enrollment_date,
			const std::string& pass_word,
			const std::string& college,
			const std::string& class_name,
			const std::string& notes, int permission_level
		);
		static void CreateTeacher(
			std::uint64_t ID,
			const std::string& name, 
			const std::string& teacher_sex, const std::string & email,
			const std::string& phone_number,
			const std::string & college,
			const std::string& pass_word, 
			const std::string & notes, int permission_level
		);
		static bool IsStudentExist(std::uint64_t id);
		static bool IsTeacherExist(std::uint64_t id);
		static void DeleteStudent(std::uint64_t student_id);
		static void DeleteTeacher(std::uint64_t teacher_id);
		static auto GetStudentInfo(std::uint64_t id)-> StudentBasicInfo;
		static auto GetTeacherInfo(std::uint64_t id) -> TeacherBasicInfo;
		static void SetStudentProperty(const StudentBasicInfo& info);
		static void SetTeacherProperty(const TeacherBasicInfo& info);
	};
	class ClassesManager
	{
	public:
		static void CreateClass(const std::string& class_name, std::uint64_t teacherID);
		static void DeleteClass(const std::string& class_name);
		static bool IsClassExist(const std::string& class_name);
	};
	class SubjectManager
	{
	public:
		static void CreateSubject(
			std::uint64_t subject_id,
			const std::string& subject_name,
			int begin_year,
			int end_year, const std::string & classroom, const std::string& description
			);
		static void AddStudent(std::uint64_t student_id, std::uint64_t subject_id, const std::string & notes);
		static void AddTeacher(std::uint64_t teacher, std::uint64_t subject_id);
		static void RemoveStudent(std::uint64_t student_id, std::uint64_t subject_id);
		static void RemoveTeacher(std::uint64_t teacher_id, std::uint64_t subject_id);
		static void DeleteSubject(std::uint64_t subject_id);
		static bool IsSubjectExist(std::uint64_t subject_id);
		static SubjectInfo GetSubjectInfo(std::uint64_t subject_id);
		static auto GetAllSubjectInfo(void) -> std::vector<SubjectInfo>;
		
	};
	struct User
	{
		std::string name;
		std::uint64_t ID;
		std::string token;
		int permission_level;
		std::time_t enable_time;
	};
	
}