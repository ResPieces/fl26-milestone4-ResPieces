#include "aiws/chunker.hpp"
#include "aiws/text_processor.hpp"

#include <stdexcept>

namespace aiws
{

    Chunker::Chunker(ChunkingPolicy policy) : policy_(policy)
    {
        if (policy_.max_tokens == 0 || policy_.overlap >= policy_.max_tokens ||
            policy_.paragraph_window > policy_.max_tokens)
        {
            throw std::invalid_argument("invalid chunking policy");
        }
    }

    /*

    Chunk Criteria
    std::string id;
    std::string document_id;
    std::size_t document_order{};
    std::size_t sequence{};
    std::string text;
    std::size_t token_count{};
    std::size_t source_begin{};
    std::size_t source_end{};
    */

    std::vector<Chunk> Chunker::chunk(const Document &document, std::size_t document_order) const
    {
        // TODO: produce deterministic, source-attributed chunks for the supplied document.
        std::vector<Chunk> chunks;
        std::string curText = "";

        std::vector<TokenInfo> tokens = TextProcessor::tokenize(document.text());

        std::size_t curSequence = 0;
        std::size_t curStart = 0;

        while (curStart < tokens.size())
        {
            int remainingTokens = tokens.size() - curStart;

            Chunk curChunk;

            if (remainingTokens <= 120)
            {
                curChunk.id = document.id() + "#" + std::to_string(curSequence);
                curChunk.document_id = document.id();
                curChunk.document_order = document_order;
                curChunk.sequence = curSequence;
                curChunk.text = TextProcessor::join(tokens, curStart, tokens.size());
                curChunk.token_count = remainingTokens;
                curChunk.source_begin = tokens.at(curStart).begin;
                curChunk.source_end = tokens.back().end;

                chunks.push_back(curChunk);
                return chunks;
            }
            else
            {
                std::size_t curEnd = curStart + 120;

                for (std::size_t i = curStart + 100; i <= curStart + 120; i++)
                {
                    if (tokens.at(i - 1).paragraph < tokens.at(i).paragraph)
                    {
                        curEnd = i;
                    }
                }

                curChunk.id = document.id() + "#" + std::to_string(curSequence);
                curChunk.document_id = document.id();
                curChunk.document_order = document_order;
                curChunk.sequence = curSequence;
                curChunk.text = TextProcessor::join(tokens, curStart, curEnd);
                curChunk.token_count = curEnd - curStart;
                curChunk.source_begin = tokens.at(curStart).begin;
                curChunk.source_end = tokens.at(curEnd - 1).end;

                chunks.push_back(curChunk);

                curStart = curEnd - 20;
                curSequence++;
            }
        }

        return chunks;
    }

} // namespace aiws
