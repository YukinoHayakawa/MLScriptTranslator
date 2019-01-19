#pragma once

#include <string>
#include <vector>

#include <utfcpp/utf8.h>

#include <Usagi/Utility/Stream.hpp>
#include <Usagi/Utility/Noncopyable.hpp>
#include <Usagi/Core/Logging.hpp>

#include "Token.hpp"

class Tokenizer : usagi::Noncopyable
{
    std::u32string mSource;
    std::vector<Token> mTokens;
    int mCurrentLine = 1;
    int mCurrentColumn = 1;
    std::size_t mCurrentPos = 0;
    bool mPlainText = true;
    bool mComment = false;
    Token mTempToken;
    std::size_t mTempPos = 0;

public:
    explicit Tokenizer(std::istream &in)
    {
        const auto u8src = usagi::readStreamAsString(in);
        utf8::utf8to32(
            u8src.begin(), u8src.end(),
            back_inserter(mSource)
        );
    }

    void beginToken()
    {
        mTempToken.line = mCurrentLine;
        mTempToken.column = mCurrentColumn;
        mTempPos = mCurrentPos;
    }

    void endToken(TokenType token, std::size_t length)
    {
        mTempToken.type = token;
        mTempToken.text = { mSource.data() + mTempPos, length };
        mTokens.push_back(mTempToken);
    }

    const char32_t & cur() const { return mSource[mCurrentPos]; }

    const char32_t & advance()
    {
        assert(mCurrentPos < mSource.size());

        // assuming unix end-line
        if(cur() == '\n')
        {
            beginToken();
            endToken(TokenType::NEWLINE, 0);

            ++mCurrentLine;
            mCurrentColumn = 1;
        }
        else
        {
            ++mCurrentColumn;
        }

        ++mCurrentPos;
        // if(mCurrentPos == mSource.size())
        //     LOG(info, "Toknenizer: Reached end-of-file");

        return cur();
    }

    char32_t peek() const
    {
        if(mCurrentPos + 1 >= mSource.size())
            return 0;
        return mSource[mCurrentPos + 1];
    }

    static bool isTokenChar(char32_t c)
    {
        return c == '['
            || c == ']'
            || isCommandTokenChar(c)
            // these are only effective within [] and {} pairs
            || c == ','
            || c == '='
            || c == ':'
        ;
    }

    static bool isCommandTokenChar(char32_t c)
    {
        return c == '{'
            || c == '}'
        ;
    }

    void skipWhiteSpace()
    {
        while(isspace(cur())) advance();
    }

    void readStringLiteral()
    {
        skipWhiteSpace();
        beginToken();
        std::size_t trim_end = 0;
        bool read_next = true;
        // if not in command block, read til end of line, otherwise read til
        // next control token
        while(peek() != '\n')
        {
            if(peek() == 0)
            {
                read_next = false;
                break;
            }
            // next is a token char, the last char is the end of string literal
            if(mPlainText)
            {
                if(!mComment && isCommandTokenChar(peek())) break;
            }
            else
            {
                if(isTokenChar(peek())) break;
            }
            // move onto next char
            advance();
            if(mComment && cur() == '}' && peek() == '}')
            {
                read_next = false;
                break;
            }

            if(isspace(cur()))
                ++trim_end;
            else
                trim_end = 0;
        }
        const auto len = mCurrentPos - mTempPos - trim_end + read_next;
        if(len > 0)
        {
            endToken(TokenType::STRING_LITERAL, len);
        }
        if(read_next) advance();
    }

    void error()
    {
        LOG(error, "at Line {}, Col {}", mCurrentLine, mCurrentColumn);
        exit(1);
    }

    void dumpTokens()
    {
        std::string t;
        for(auto &&token : mTokens)
        {
            t.clear();
            utf8::utf32to8(
                token.text.begin(), token.text.end(),
                std::back_inserter(t)
            );
            LOG(info, "Line {}, Col {}: [{}] {}",
                token.line, token.column,
                tokenName(token.type),
                t
            );
        }
    }

    void tokenize()
    {
        while(mCurrentPos != mSource.size())
        {
            skipWhiteSpace();
            beginToken();
            if(cur() == '[')
            {
                endToken(TokenType::LEFT_BRACKET, 1);
                mPlainText = false;
                advance();
                continue;
            }
            if(cur() == ']')
            {
                endToken(TokenType::RIGHT_BRACKET, 1);
                mPlainText = true;
                advance();
                continue;
            }
            if(cur() == '{')
            {
                if(peek() == '{')
                {
                    advance();
                    endToken(TokenType::LEFT_DOUBLE_BRACE, 2);
                    mComment = true;
                    advance();
                    readStringLiteral();
                }
                else
                {
                    endToken(TokenType::LEFT_BRACE, 1);
                    mPlainText = false;
                    advance();
                }
                continue;
            }
            if(cur() == '}')
            {
                if(peek() == '}')
                {
                    advance();
                    endToken(TokenType::RIGHT_DOUBLE_BRACE, 2);
                    mComment = false;
                }
                else
                {
                    endToken(TokenType::RIGHT_BRACE, 1);
                    mPlainText = true;
                }
                advance();
                continue;
            }
            if(!mPlainText)
            {
                if(cur() == ':')
                {
                    endToken(TokenType::COLON, 1);
                    advance();
                    continue;
                }
                if(cur() == '=')
                {
                    endToken(TokenType::EQUAL, 1);
                    advance();
                    continue;
                }
                if(cur() == ',')
                {
                    endToken(TokenType::COMMA, 1);
                    advance();
                    continue;
                }
            }
            readStringLiteral();
        }
    }
};
