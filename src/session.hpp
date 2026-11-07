#pragma once
#include "trainer.hpp"
#include <functional>
#include <memory>

namespace alenka {
// Coordinates synchronous checkpoints. The trainer never performs filesystem I/O.
class Session {
public:
    Session(Trainer& trainer,const std::filesystem::path& root);
    // Returns true only after a successful close or an explicit discard/fatal dismissal.
    bool update(bool close_requested=false);
private:
    Trainer& trainer_;
    std::unique_ptr<ProfileStore> store_;
    std::function<void()> pending_;
    std::string selected_id_;
    bool closing_=false,done_=false;
    void save_current();
    void execute();
};
}
