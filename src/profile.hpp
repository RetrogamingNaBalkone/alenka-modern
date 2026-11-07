#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace alenka {
enum class CourseLanguage { Russian, English };
struct CharacterCount { std::uint64_t ticks=0, correct=0, errors=0; };
struct CourseProgress {
    unsigned line=0;
    std::array<CharacterCount,256> counts{};
    std::uint64_t total_count=0, total_ticks=0;
};
struct TrainingSettings {
    unsigned flags=0x56, hint=0, rhythm=30;
    bool sound=true;
};
struct Profile {
    std::string id, name=u8"Игрок";
    CourseLanguage language=CourseLanguage::Russian;
    TrainingSettings settings;
    std::array<CourseProgress,2> courses{};
};
// Strict UTF-8 and the original CP866 alphabet, shared by editing and storage.
std::string name_glyphs(const std::string& utf8);
std::string normalize_name(const std::string& utf8);
std::string name_key(const std::string& utf8);
std::string new_profile_id();

class ProfileStore {
public:
    explicit ProfileStore(const std::filesystem::path& root,std::array<unsigned,2> course_lengths={1000,1000});
    ~ProfileStore();
    ProfileStore(const ProfileStore&)=delete;
    ProfileStore& operator=(const ProfileStore&)=delete;
    std::vector<Profile> list(std::string& warning) const;
    Profile startup(std::string& warning);
    void save(const Profile& profile);
    void select(const std::string& id);
private:
    std::filesystem::path root_;
    std::array<unsigned,2> course_lengths_;
    std::intptr_t lock_=-1;
};
}
