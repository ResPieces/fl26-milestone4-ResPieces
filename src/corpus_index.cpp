#include "aiws/corpus_index.hpp"
#include <stdexcept>
#include "aiws/text_processor.hpp"

#include <iostream>

namespace aiws
{

    CorpusIndex::CorpusIndex(const std::vector<Chunk> &chunks)
    {
        build(chunks);
    }

    void CorpusIndex::build(const std::vector<Chunk> &chunks)
    {
        // TODO: build the searchable index from the supplied chunks.

        // Ensure everything is clear
        chunk_by_id_.clear();
        postings_.clear();

        // Loop through each chunk
        for (std::size_t i = 0; i < chunks.size(); i++)
        {

            Chunk curChunk = chunks.at(i);
            std::vector<TokenInfo> tokens = TextProcessor::tokenize(curChunk.text);

            chunk_by_id_[curChunk.id] = i;
            // std::cout << "Adding chunk ID: " << curChunk.id << " at index: " << i << std::endl;

            // Loop through each Token
            for (TokenInfo &token : tokens)
            {

                auto it = postings_.find(token.token);

                // See if the token is already in postings_
                if (it != postings_.end())
                {

                    bool entryFound = false;

                    // Second is a vector of Posting
                    // Contains chunk index (i) and frequency
                    for (auto &posting : it->second)
                    {
                        if (posting.chunk_index == i)
                        {
                            posting.frequency++;
                            entryFound = true;
                        }
                    }

                    // Case for if the token has been found in another chunk but not the current one
                    if (!entryFound)
                    {
                        postings_[token.token].push_back({i, 1});
                    }
                }
                else
                {
                    // Handles case of finding a completely new token
                    postings_[token.token].push_back({i, 1});
                }
            }
        }
    }

    std::size_t CorpusIndex::document_frequency(
        const std::string &normalized_term) const noexcept
    {
        // TODO: return how many chunks contain the requested term.
        std::vector<std::string> normalizedTermVector = TextProcessor::terms(normalized_term);
        if (normalizedTermVector.size() > 1)
        {
            throw std::invalid_argument("Term normalizes to more than one token");
        }

        auto it = postings_.find(normalizedTermVector.at(0));

        if (it != postings_.end())
        {
            return it->second.size();
        }

        return 0;
    }

    std::size_t CorpusIndex::term_frequency(
        const std::string &normalized_term,
        const std::string &chunk_id) const noexcept
    {
        // TODO: return the requested term's frequency in the specified chunk.

        std::vector<std::string> normalizedTermVector = TextProcessor::terms(normalized_term);
        if (normalizedTermVector.size() > 1)
        {
            throw std::invalid_argument("Term normalizes to more than one token");
        }

        auto it = postings_.find(normalizedTermVector.at(0));

        if (it != postings_.end())
        {

            // std::cout << "Searching for chunk ID: " << chunk_id << std::endl;

            auto chunkIt = chunk_by_id_.find(chunk_id);

            // std::cout << "Found: " << (chunkIt != chunk_by_id_.end()) << std::endl;

            // Checks to see if the ID of the chunk we are looking at is in our postings
            if (chunkIt != chunk_by_id_.end())
            {
                for (auto &posting : it->second)
                {
                    // std::cout << "In Loop" << std::endl;
                    if (posting.chunk_index == chunkIt->second)
                    {
                        return posting.frequency;
                    }
                }
            }
        }

        return 0;
    }

    const std::vector<CorpusIndex::Posting> *CorpusIndex::postings(
        const std::string &normalized_term) const noexcept
    {
        // TODO: return the postings associated with the requested term.
        std::vector<std::string> normalizedTermVector = TextProcessor::terms(normalized_term);

        // Assumes there is no issues with repeated postings due to given structure of function
        if (normalizedTermVector.empty())
        {
            return nullptr;
        }

        auto it = postings_.find(normalizedTermVector.at(0));

        if (it != postings_.end())
        {
            return &it->second;
        }

        return nullptr;
    }

    const Chunk *CorpusIndex::find_chunk(
        const std::vector<Chunk> &chunks,
        const std::string &chunk_id) const noexcept
    {
        // TODO: find the chunk identified by the requested chunk ID.
        for (const Chunk &chunk : chunks)
        {
            if (chunk.id == chunk_id)
            {
                return &chunk;
            }
        }

        return nullptr;
    }

    std::size_t CorpusIndex::chunk_index(const std::string &chunk_id) const
    {
        // TODO: return the stored index of the requested chunk ID.

        for (std::size_t i = 0; i < chunk_by_id_.size(); i++)
        {
            auto it = chunk_by_id_.find(chunk_id);
            return it->second;
        }
        return 0;
    }

} // namespace aiws
