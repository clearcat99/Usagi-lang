// Usagi-lang (.yaha) - UTF-8 with BOM supported; C++17
// Ensure MSVC encodes narrow string literals as UTF-8, even without /utf-8.
#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using Words = std::vector<std::string>;

struct Language
{
    std::string Number = "야하";
    std::string Negative = "이";
    std::string Print = "미이이야하";
    std::string Equal = "푸루";
    std::string Less = "푸루루";
    std::string Greater = "푸루루루";
    std::string Add = "푸릿";
    std::string Subtract = "푸리릿";
    std::string Multiply = "푸리리릿";
    std::string Divide = "푸리리리릿";
    std::string Return = "푸루야";
    std::string BlockStart = "하!";
    std::string BlockEnd = "하?";
    std::string FunctionStart = "우라-야하!";
    std::string FunctionPrefix = "하";
    std::string FunctionUnit = "아";
    std::string FunctionDeclSuffix = "!";
    std::string FunctionEndSuffix = "?";
    std::string FunctionCallSuffix = "-";
    std::string Assignment = "미야아아";
};

static bool BeginsWith(const std::string& Text, const std::string& Prefix)
{
    return !Prefix.empty() && Text.compare(0, Prefix.size(), Prefix) == 0;
}

static void RemoveBom(std::string& Text)
{
    if (BeginsWith(Text, "\xEF\xBB\xBF"))
    {
        Text.erase(0, 3);
    }
}

static std::string ReadFile(const std::filesystem::path& Path)
{
    std::ifstream Stream(Path, std::ios::binary);
    if (!Stream)
    {
        throw std::runtime_error("파일을 열 수 없습니다: " + Path.string());
    }
    std::ostringstream Buffer;
    Buffer << Stream.rdbuf();
    std::string Result = Buffer.str();
    RemoveBom(Result);
    return Result;
}

static Words Tokenize(const std::string& Source)
{
    Words Result;
    std::istringstream Input(Source);
    std::string Line;
    while (std::getline(Input, Line))
    {
        const auto Comment = Line.find('#');
        if (Comment != std::string::npos)
        {
            Line.erase(Comment);
        }
        std::istringstream Part(Line);
        std::string Word;
        while (Part >> Word)
        {
            Result.push_back(Word);
        }
    }
    return Result;
}

static void LoadLanguage(Language& Lang, const std::filesystem::path& Path)
{
    if (Path.empty())
    {
        return;
    }
    std::string Data = ReadFile(Path);
    std::istringstream Input(Data);
    std::string Line;
    const std::unordered_map<std::string, std::string Language::*> Fields = {
        {"Number", &Language::Number}, {"Negative", &Language::Negative},
        {"Print", &Language::Print}, {"Equal", &Language::Equal},
        {"Less", &Language::Less}, {"Greater", &Language::Greater},
        {"Add", &Language::Add}, {"Subtract", &Language::Subtract},
        {"Multiply", &Language::Multiply}, {"Divide", &Language::Divide},
        {"Return", &Language::Return}, {"BlockStart", &Language::BlockStart},
        {"BlockEnd", &Language::BlockEnd}, {"FunctionStart", &Language::FunctionStart},
        {"FunctionPrefix", &Language::FunctionPrefix},
        {"FunctionUnit", &Language::FunctionUnit},
        {"FunctionDeclSuffix", &Language::FunctionDeclSuffix},
        {"FunctionEndSuffix", &Language::FunctionEndSuffix},
        {"FunctionCallSuffix", &Language::FunctionCallSuffix},
        {"Assignment", &Language::Assignment}
    };
    while (std::getline(Input, Line))
    {
        const auto Comment = Line.find('#');
        if (Comment != std::string::npos)
        {
            Line.erase(Comment);
        }
        const auto Pos = Line.find('=');
        if (Pos == std::string::npos)
        {
            continue;
        }
        auto Trim = [](std::string S)
        {
            const auto First = S.find_first_not_of(" \t\r\n");
            if (First == std::string::npos)
            {
                return std::string {};
            }
            const auto Last = S.find_last_not_of(" \t\r\n");
            return S.substr(First, Last - First + 1);
        };
        std::string Key = Trim(Line.substr(0, Pos));
        std::string Value = Trim(Line.substr(Pos + 1));
        auto Found = Fields.find(Key);
        if (Found != Fields.end() && !Value.empty())
        {
            Lang.*(Found->second) = Value;
        }
    }
}

class Interpreter
{
public:
    explicit Interpreter(Language Lang) : Lang(std::move(Lang)) {}

    void Run(const std::string& Source)
    {
        Words Tokens = Tokenize(Source);
        RegisterFunctions(Tokens);
        std::size_t Position = 0;
        Execute(Tokens, Position, Tokens.size(), 0);
    }

private:
    Language Lang;
    std::unordered_map<std::string, long long> Variables;
    struct Function
    {
        std::vector<std::string> Parameters;
        Words Body;
    };
    std::unordered_map<int, Function> Functions;
    static constexpr int MaxDepth = 100;

    int FunctionIndex(const std::string& Word, const std::string& Suffix) const
    {
        if (Suffix.empty())
        {
            throw std::runtime_error("함수 접미사가 비어 있습니다.");
        }
        if (!BeginsWith(Word, Lang.FunctionPrefix))
        {
            return 0;
        }

        std::string Rest = Word.substr(Lang.FunctionPrefix.size());
        int Index = 0;

        while (BeginsWith(Rest, Lang.FunctionUnit))
        {
            Rest = Rest.substr(Lang.FunctionUnit.size());
            ++Index;
        }

        if (Rest != Suffix)
        {
            return 0;
        }
        return Index;
    }

    bool IsVariable(const std::string& Word) const
    {
        if (!BeginsWith(Word, "우"))
        {
            return false;
        }
        std::string Rest = Word;
        int Count = 0;
        for (std::size_t Pos = 0; Pos < Rest.size();)
        {
            std::string Character = Rest.substr(Pos, 3);
            if (Character != "우" && Character != "라" && Character != "아")
            {
                return false;
            }
            Pos += 3;
            ++Count;
        }
        return Count > 0;
    }

    std::string VariableName(const std::string& Word) const
    {
        return "V" + std::to_string(Word.size() / 3);
    }

    bool IsComparison(const std::string& Word) const
    {
        return Word == Lang.Equal || Word == Lang.Less || Word == Lang.Greater;
    }

    bool TryNumber(const std::string& Word, long long& Value) const
    {
        std::string Rest = Word;
        int Negatives = 0;
        while (BeginsWith(Rest, Lang.Negative))
        {
            Rest.erase(0, Lang.Negative.size());
            ++Negatives;
        }
        if (!BeginsWith(Rest, Lang.Number))
        {
            return false;
        }
        Rest.erase(0, Lang.Number.size());
        if (!std::all_of(Rest.begin(), Rest.end(), [](char C) { return C == '!'; }))
        {
            return false;
        }
        Value = static_cast<long long>(Rest.size());
        if (Negatives % 2)
        {
            Value = -Value;
        }
        return true;
    }

    std::string Take(const Words& Tokens, std::size_t& Position, std::size_t End) const
    {
        if (Position >= End)
        {
            throw std::runtime_error("필요한 값이 없습니다.");
        }
        return Tokens[Position++];
    }

    bool IsArithmetic(const std::string& Word) const
    {
        return Word == Lang.Add || Word == Lang.Subtract ||
               Word == Lang.Multiply || Word == Lang.Divide;
    }

    bool IsExpression(const std::string& Word) const
    {
        long long Unused = 0;
        return TryNumber(Word, Unused) || IsVariable(Word) ||
               IsArithmetic(Word) || FunctionIndex(Word, Lang.FunctionCallSuffix) > 0;
    }

    long long Calculate(const std::string& Operator, long long Left, long long Right) const
    {
        if (Operator == Lang.Add)
        {
            return Left + Right;
        }
        if (Operator == Lang.Subtract)
        {
            return Left - Right;
        }
        if (Operator == Lang.Multiply)
        {
            return Left * Right;
        }
        if (Right == 0)
        {
            throw std::runtime_error("0으로 나눌 수 없습니다.");
        }
        return Left / Right;
    }

    long long ReadExpression(const Words& Tokens, std::size_t& Position,
                             std::size_t End, int Depth, bool Evaluate = true)
    {
        if (Depth > MaxDepth)
        {
            throw std::runtime_error("표현식/함수 중첩 깊이 제한 초과");
        }
        const std::string Word = Take(Tokens, Position, End);
        if (IsArithmetic(Word))
        {
            const long long Left = ReadExpression(Tokens, Position, End, Depth + 1, Evaluate);
            const long long Right = ReadExpression(Tokens, Position, End, Depth + 1, Evaluate);
            return Evaluate ? Calculate(Word, Left, Right) : 0;
        }
        const int Index = FunctionIndex(Word, Lang.FunctionCallSuffix);
        if (Index > 0)
        {
            auto Found = Functions.find(Index);
            if (Found == Functions.end())
            {
                throw std::runtime_error("정의되지 않은 함수 호출: " + Word);
            }
            const Function& Definition = Found->second;

            std::unordered_map<std::string, long long> LocalVariables;
            for (const std::string& Parameter : Definition.Parameters)
            {
                const long long Argument = ReadExpression(Tokens, Position, End, Depth + 1, Evaluate);
                LocalVariables[Parameter] = Argument;
            }
            if (!Evaluate)
            {
                return 0;
            }

            auto CallerVariables = Variables;
            Variables = std::move(LocalVariables);
            try
            {
                std::size_t FunctionPosition = 0;
                auto Result = Execute(Definition.Body, FunctionPosition, Definition.Body.size(), Depth + 1);
                Variables = std::move(CallerVariables);
                return Result.value_or(0);
            }
            catch (...)
            {
                Variables = std::move(CallerVariables);
                throw;
            }
        }
        long long Value = 0;
        if (TryNumber(Word, Value))
        {
            return Evaluate ? Value : 0;
        }
        if (IsVariable(Word))
        {
            return Evaluate ? Variables[VariableName(Word)] : 0;
        }
        throw std::runtime_error("숫자, 변수 또는 표현식이 아닙니다: " + Word);
    }

    std::size_t FindEnd(const Words& Tokens, std::size_t Start, const std::string& Open,
                        const std::string& Close, std::size_t End)
    {
        int Depth = 1;
        for (std::size_t I = Start; I < End; ++I)
        {
            if (Tokens[I] == Open)
            {
                ++Depth;
            }
            if (Tokens[I] == Close && --Depth == 0)
            {
                return I;
            }
        }
        throw std::runtime_error("블록의 닫는 키워드가 없습니다: " + Close);
    }

    void RegisterFunctions(Words& Tokens)
    {
        Words Main;
        for (std::size_t I = 0; I < Tokens.size();)
        {
            if (Tokens[I] != Lang.FunctionStart)
            {
                Main.push_back(Tokens[I++]);
                continue;
            }
            ++I;
            if (I >= Tokens.size())
            {
                throw std::runtime_error("함수 선언 이름이 없습니다.");
            }

            const int Index = FunctionIndex(Tokens[I++], Lang.FunctionDeclSuffix);
            if (Index == 0)
            {
                throw std::runtime_error("함수 선언 이름이 잘못되었습니다: " + Tokens[I - 1]);
            }

            Function Definition;
            while (I < Tokens.size() && IsVariable(Tokens[I]))
            {
                const std::string Parameter = VariableName(Tokens[I++]);
                if (std::find(Definition.Parameters.begin(), Definition.Parameters.end(), Parameter)
                    != Definition.Parameters.end())
                {
                    throw std::runtime_error("매개변수 번호가 중복됩니다.");
                }
                Definition.Parameters.push_back(Parameter);
            }

            const std::size_t BodyStart = I;
            while (I < Tokens.size() && FunctionIndex(Tokens[I], Lang.FunctionEndSuffix) == 0)
            {
                if (Tokens[I] == Lang.FunctionStart)
                {
                    throw std::runtime_error("중첩 함수 선언은 지원하지 않습니다.");
                }
                ++I;
            }
            if (I == Tokens.size())
            {
                throw std::runtime_error("함수가 닫히지 않았습니다.");
            }

            if (FunctionIndex(Tokens[I], Lang.FunctionEndSuffix) != Index)
            {
                throw std::runtime_error("함수 선언과 종료의 번호가 다릅니다.");
            }

            Definition.Body = Words(Tokens.begin() + BodyStart, Tokens.begin() + I);
            Functions[Index] = std::move(Definition);
            ++I;
        }
        Tokens = std::move(Main);
    }

    bool Compare(const std::string& Operator, long long Left, long long Right) const
    {
        if (Operator == Lang.Equal)
        {
            return Left == Right;
        }
        if (Operator == Lang.Less)
        {
            return Left < Right;
        }
        return Left > Right;
    }

    std::optional<long long> Execute(const Words& Tokens, std::size_t& Position,
                                     std::size_t End, int Depth)
    {
        if (Depth > MaxDepth)
        {
            throw std::runtime_error("함수 호출 깊이 제한 초과");
        }
        while (Position < End)
        {
            const std::string Op = Take(Tokens, Position, End);
            if (Op == Lang.BlockStart)
            {
                const std::size_t Close = FindEnd(Tokens, Position, Lang.BlockStart, Lang.BlockEnd, End);
                std::size_t Start = Position;
                auto Result = Execute(Tokens, Start, Close, Depth + 1);
                if (Result)
                {
                    return Result;
                }
                Position = Close + 1;
            }
            else if (Op == Lang.BlockEnd || Op == Lang.FunctionEndSuffix)
            {
                throw std::runtime_error("예상하지 못한 블록 종료: " + Op);
            }
            else if (Op == Lang.Print)
            {
                std::cout << ReadExpression(Tokens, Position, End, Depth + 1) << '\n';
            }
            else if (Op == Lang.Return)
            {
                if (Position < End && IsExpression(Tokens[Position]))
                {
                    return ReadExpression(Tokens, Position, End, Depth + 1);
                }
                return 0;
            }
            else if (Op == Lang.Assignment)
            {
                const std::string VarToken = Take(Tokens, Position, End);
                if (!IsVariable(VarToken))
                {
                    throw std::runtime_error("대입 연산자 뒤에는 변수가 와야 합니다: " + VarToken);
                }
                const long long Value = ReadExpression(Tokens, Position, End, Depth + 1);
                Variables[VariableName(VarToken)] = Value;
            }
            else if (FunctionIndex(Op, Lang.FunctionCallSuffix) > 0 || IsArithmetic(Op))
            {
                --Position;
                (void)ReadExpression(Tokens, Position, End, Depth + 1);
            }
            else if (IsComparison(Op))
            {
                Words Comparisons = {Op};
                while (Position < End && IsComparison(Tokens[Position]))
                {
                    Comparisons.push_back(Take(Tokens, Position, End));
                }
                const long long Left = ReadExpression(Tokens, Position, End, Depth + 1);
                const long long Right = ReadExpression(Tokens, Position, End, Depth + 1);
                bool Result = false;
                for (const std::string& Comparison : Comparisons)
                {
                    Result = Compare(Comparison, Left, Right) || Result;
                }

                if (Position >= End)
                {
                    throw std::runtime_error("조건문의 본문이 없습니다.");
                }

                if (Result)
                {
                    if (Tokens[Position] == Lang.BlockStart)
                    {
                        const std::size_t Close = FindEnd(Tokens, Position + 1, Lang.BlockStart, Lang.BlockEnd, End);
                        std::size_t Start = Position + 1;
                        auto ExecResult = Execute(Tokens, Start, Close, Depth + 1);
                        if (ExecResult)
                        {
                            return ExecResult;
                        }
                        Position = Close + 1;
                    }
                }
                else
                {
                    if (Tokens[Position] == Lang.BlockStart)
                    {
                        const std::size_t Close = FindEnd(Tokens, Position + 1, Lang.BlockStart, Lang.BlockEnd, End);
                        Position = Close + 1;
                    }
                    else
                    {
                        SkipStatement(Tokens, Position, End, Depth + 1);
                    }
                }
            }
            else
            {
                throw std::runtime_error("알 수 없는 키워드: " + Op);
            }
        }
        return std::nullopt;
    }

    void SkipStatement(const Words& Tokens, std::size_t& Position,
                       std::size_t End, int Depth)
    {
        const std::string Op = Take(Tokens, Position, End);
        if (Op == Lang.Assignment)
        {
            ++Position;
            ReadExpression(Tokens, Position, End, Depth + 1, false);
        }
        else if (Op == Lang.Print || FunctionIndex(Op, Lang.FunctionCallSuffix) > 0 || IsArithmetic(Op))
        {
            --Position;
            if (Op == Lang.Print)
            {
                ++Position;
            }
            ReadExpression(Tokens, Position, End, Depth + 1, false);
        }
        else if (Op == Lang.Return)
        {
            if (Position < End && IsExpression(Tokens[Position]))
            {
                ReadExpression(Tokens, Position, End, Depth + 1, false);
            }
        }
        else
        {
            throw std::runtime_error("단일 조건문의 본문은 출력/할당/함수호출/return 또는 블록이어야 합니다.");
        }
    }
};

int main(int Argc, char** Argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    try
    {
        Language Lang;
        std::filesystem::path FilePath;
        std::filesystem::path LanguagePath;
        for (int I = 1; I < Argc; ++I)
        {
            std::string Arg = Argv[I];
            if (Arg == "--lang" && I + 1 < Argc)
            {
                LanguagePath = Argv[++I];
            }
            else if (FilePath.empty())
            {
                FilePath = Arg;
            }
            else
            {
                throw std::runtime_error("지원하지 않는 인자: " + Arg);
            }
        }
        LoadLanguage(Lang, LanguagePath);
        if (!FilePath.empty())
        {
            if (FilePath.extension() != ".yaha")
            {
                std::cerr << "주의: .yaha 확장자를 권장합니다.\n";
            }
            Interpreter(Lang).Run(ReadFile(FilePath));
            return 0;
        }
        std::cout << "Usagi-lang (.yaha) / UTF-8\n"
                     "코드 입력 후 빈 줄: 실행 / :exit: 종료\n\n";
        Interpreter Repl(Lang);
        std::string Source, Line;
        while (true)
        {
            std::cout << (Source.empty() ? ">>> " : "... ") << std::flush;
            if (!std::getline(std::cin, Line) || Line == ":exit")
            {
                break;
            }
            if (!Line.empty())
            {
                Source += Line + '\n';
                continue;
            }
            if (Source.empty())
            {
                continue;
            }
            try
            {
                Repl.Run(Source);
            }
            catch (const std::exception& Error)
            {
                std::cerr << "오류: " << Error.what() << '\n';
            }
            Source.clear();
        }
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "오류: " << Error.what() << '\n';
        return 1;
    }
}
