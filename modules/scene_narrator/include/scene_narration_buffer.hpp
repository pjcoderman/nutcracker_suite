#pragma once

#include "llama.h"

#include <iterator>
#include <string>
#include <vector>

#include <iostream>
#include <streambuf>

#define NARRATOR_CONTEXT_WINDOW_SIZE_DEFAULT 16384

namespace scene_narrator {

    class SceneNarrationBuffer: public std::streambuf {
        
        private:
        llama_model *model;
        llama_context *ctx;
        llama_batch batch;
        std::ostream * debug_log;

        const llama_vocab * vocab;
        
        llama_sampler *sampler;
        std::string pending;
        llama_token token_id;
        bool done;
        
        enum NarrationState { CHECKING_THINK, IN_THINK, IN_NARRATE };
        NarrationState state;
        bool content_encountered;

        public:
        SceneNarrationBuffer(llama_model* model, llama_context *ctx, llama_batch batch, std::ostream *debug_log);
        ~SceneNarrationBuffer();

        protected:
        int_type underflow() override;

        private:
        
        std::string read_next_token();
        void set_state(NarrationState state);
        void emit_debug_state_header(std::string_view state_str);
        void emit_debug_content(std::string_view content);
        void capture_pre_think_content(std::string_view content);
        void capture_think_content(std::string_view content);
        void capture_narration_content(std::string_view content);
    };

}