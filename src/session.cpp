#include "session.hpp"
#include <algorithm>
#include <stdexcept>

namespace alenka {
Session::Session(Trainer& trainer,const std::filesystem::path& root):trainer_(trainer) {
    try {
        store_=std::make_unique<ProfileStore>(root,trainer.course_lengths());
    } catch(const std::exception&) {
        trainer_.notify(u8"Папка недоступна или открыта другим экземпляром",true);return;
    }
    Profile fallback;fallback.id=new_profile_id();trainer_.load_profile(fallback);
    pending_=[this] {
        std::string warning;auto p=store_->startup(warning);
        trainer_.load_profile(p);store_->select(p.id);selected_id_=p.id;
        if(!warning.empty()) trainer_.notify(warning);
    };
    execute();
}
void Session::save_current() {
    auto p=trainer_.profile();store_->save(p);
    if(selected_id_!=p.id) { store_->select(p.id);selected_id_=p.id; }
    trainer_.saved();
}
void Session::execute() {
    try { pending_();pending_={};trainer_.dismiss_error(); }
    catch(const std::exception&) { trainer_.show_error(closing_); }
}
bool Session::update(bool close_requested) {
    using Kind=Trainer::ActionKind;
    if(!trainer_.running()) return true;
    auto action=trainer_.take_action();
    if(action.kind==Kind::Discard&&closing_) return true;
    if(action.kind==Kind::Cancel) { pending_={};closing_=false;trainer_.saved();return false; }
    if(action.kind==Kind::Retry) { if(pending_) execute();return done_; }
    if(close_requested) {
        if(!store_) return true;
        closing_=true;pending_=[this]{save_current();done_=true;};execute();return done_;
    }
    if(pending_||!store_) return false;
    closing_=false;
    if(action.kind==Kind::List) {
        pending_=[this] {
            save_current();std::string warning;auto profiles=store_->list(warning);
            trainer_.show_profiles(profiles,warning);
        };
    } else if(action.kind==Kind::Rename||action.kind==Kind::Create) {
        auto candidate=action.kind==Kind::Create?Profile{}:trainer_.profile();
        candidate.name=action.value;
        if(action.kind==Kind::Create) candidate.id=new_profile_id();
        // Candidate identity is fixed before the first write, so retries cannot
        // create duplicate files if selecting it fails after a successful save.
        pending_=[this,candidate] {
            save_current();std::string warning;auto profiles=store_->list(warning);
            for(const auto& p:profiles) if(p.id!=candidate.id&&name_key(p.name)==name_key(candidate.name)) {
                trainer_.edit_error(u8"Профиль с таким именем уже существует");return;
            }
            trainer_.validate_profile(candidate);store_->save(candidate);
            if(selected_id_!=candidate.id) { store_->select(candidate.id);selected_id_=candidate.id; }
            trainer_.load_profile(candidate);
        };
    } else if(action.kind==Kind::Switch) {
        pending_=[this,id=action.value] {
            save_current();std::string warning;auto profiles=store_->list(warning);
            auto found=std::find_if(profiles.begin(),profiles.end(),[&](const Profile& p){return p.id==id;});
            if(found==profiles.end()) { trainer_.notify(u8"Выбранный профиль недоступен");return; }
            trainer_.validate_profile(*found);store_->select(found->id);selected_id_=found->id;trainer_.load_profile(*found);
        };
    } else if(trainer_.save_needed()) pending_=[this]{save_current();};
    if(pending_) execute();
    return done_;
}
}
