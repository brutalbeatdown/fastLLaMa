#if !defined(FAST_LLAMA_LLAMA_HPP)
#define FAST_LLAMA_LLAMA_HPP

#include "ggml.h"
#include "vocab.hpp"
#include "model_type.hpp"
#include <cstdint>
#include <vector>
#include <thread>
#include <unordered_map>
#include "logger.hpp"
#include "span.hpp"

namespace fastllama {

    inline static constexpr std::uint32_t magic_number_v = 0x67676d6c;
    inline static constexpr std::uint32_t file_version_v = 1;
    
    struct Layer {
        // normalization
        ggml_tensor * attention_norm;

        // attention
        ggml_tensor * wq;
        ggml_tensor * wk;
        ggml_tensor * wv;
        ggml_tensor * wo;

        // normalization
        ggml_tensor * ffn_norm;

        // ff
        ggml_tensor * w1;
        ggml_tensor * w2;
        ggml_tensor * w3;
    };

    struct HyperParams {
        std::int32_t n_vocab { 32000 };
        std::int32_t n_ctx   { 512 };
        std::int32_t n_embd  { 4096 };
        std::int32_t n_mult  { 256 };
        std::int32_t n_head  { 32 };
        std::int32_t n_layer { 32 };
        std::int32_t n_rot   { 64 };
        std::int32_t f16     { 1 };
    };

    // struct Perplexity {
    //     std::size_t start;
    //     std::size_t end;
    //     double perplexity;
    // };

    struct KVCacheBuffer {

        bool init(HyperParams const& params, Logger const& logger = Logger{});
        void deinit(Logger const& logger = Logger{});

        ggml_type memory_type{ GGML_TYPE_F32 };

        // key + value memory
        ggml_tensor * k;
        ggml_tensor * v;

        ggml_context * ctx;

        std::vector<unsigned char> buffer;

        std::size_t number_of_tokens_in_cache{};
    };

    struct Model {
        using vocab_id = typename Vocab::id;

        static constexpr std::size_t max_number_of_scratch_buffer = 16;
    #ifdef LLAMA_USE_SCRATCH
        static constexpr bool use_scratch_buffer = true;
    #else
        static constexpr bool use_scratch_buffer = false;
    #endif

        bool load(std::string_view model_name, std::string_view filepath, bool disable_version_check = false);
        auto unload() -> void;
        auto eval(
            std::size_t                     n_past,
            Span<vocab_id>                  embd_inp,
            std::vector<float>&             embd_w,
            std::size_t&                    mem_per_token
        ) -> bool;

        auto set_threads(int in_threads) noexcept {
            this->threads = std::max(1, std::min(static_cast<int>(std::thread::hardware_concurrency()), in_threads));
        }

        bool dump_vocab(std::string_view filepath);

        void use_buf([[maybe_unused]] ggml_context* in_ctx, [[maybe_unused]] int i) {
            if constexpr (use_scratch_buffer) {
                auto last_size = std::size_t{};

                if (i == -1) {
                    last_size = ggml_set_scratch(in_ctx, { 0, 0, nullptr });
                } else {
                    auto buff = buf_scratch[i];
                    last_size = ggml_set_scratch(in_ctx, { 0, buff.size(), buff.data() });
                }

                if (last_size > 0) buf_max_size[buf_last] = std::max(buf_max_size[buf_last], last_size);
                
                buf_last = i;
            }
        }

        std::size_t get_buf_max_mem(int i) {
            if constexpr(use_scratch_buffer) {
                return buf_max_size[i];
            } else {
                return 0;
            }
        }

        Logger logger{};

        ModelId model_id{};
        Vocab vocabulary;

        HyperParams params;

        ggml_tensor* tok_embeddings;
        
        ggml_tensor * norm;
        ggml_tensor * output;

        std::vector<Layer> layers;
        std::vector<float> embeddings;

        ggml_context * ctx;
        std::vector<unsigned char> buffer;

        KVCacheBuffer kv_self{};

        std::vector<unsigned char> buf_compute;
        std::vector<unsigned char> buf_scratch[max_number_of_scratch_buffer];

        int    buf_last = 0;
        std::size_t buf_max_size[max_number_of_scratch_buffer] = { 0 };
        std::size_t allocate_extra_mem{};

        std::unordered_map<std::string, ggml_tensor*> tensors;

        bool is_valid{false};
        bool embeddings_eval_enable{false};
        bool should_put_all_logits{false};
        int threads{ static_cast<int>(std::thread::hardware_concurrency()) };
        int n_batch{64};
    };

    bool quantize(std::string_view in_filepath, std::string_view out_filepath, int itype);

} // namespace fastllama


#endif // FAST_LLAMA_LLAMA_HPP
