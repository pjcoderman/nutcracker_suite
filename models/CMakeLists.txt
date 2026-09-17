include("${CMAKE_CURRENT_LIST_DIR}/cmake/download_model.cmake")

# 1. Mistral Small 3.1 24B (14.3 GB) - for deep strategization if ever needed (CPU Offload!)
download_model(
    "${CMAKE_CURRENT_LIST_DIR}/mistralai_Mistral-Small-3.1-24B-Instruct-2503-Q4_K_M.gguf"
    "https://huggingface.co/bartowski/mistralai_Mistral-Small-3.1-24B-Instruct-2503-GGUF/resolve/main/mistralai_Mistral-Small-3.1-24B-Instruct-2503-Q4_K_M.gguf"
)

# 2. Pony Diffusion V6 XL (2.73 GB) - for image generation
download_model(
    "${CMAKE_CURRENT_LIST_DIR}/pony_diffusion_v6_xl_Q8_0.gguf"
    "https://huggingface.co/morikomorizz/Pony-Diffusion-V6-XL-GGUF/resolve/main/pony_diffusion_v6_xl_Q8_0.gguf"
)

# 3. Qwen 3 8B (5.02 GB)
download_model(
    "${CMAKE_CURRENT_LIST_DIR}/Qwen3-8B-Q4_K_M.gguf"
    "https://huggingface.co/Qwen/Qwen3-8B-GGUF/resolve/main/Qwen3-8B-Q4_K_M.gguf"
)

# BGE Large for embeddings (runs lightning fast in memory)
download_model(
    "${CMAKE_CURRENT_LIST_DIR}/bge-large-en-v1.5-f16.gguf"
    "https://huggingface.co/CompendiumLabs/bge-large-en-v1.5-gguf/resolve/main/bge-large-en-v1.5-f16.gguf"
)
