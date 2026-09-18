/* runcpp2

RequiredProfiles:
    DefaultPlatform: ["g++"]

OverrideCompileFlags:
    DefaultPlatform:
        "g++":
        # "g++14":
            Remove: "-std=c++17"
            # Append: "-std=c++11 -Wno-sign-compare -E -P"
            # Append: "-std=c++11 -Wno-sign-compare -pg -g"
            Append: "-std=c++11 -Wno-sign-compare -Wno-narrowing -O0"

# OverrideLinkFlags:
#     DefaultPlatform:
#         "g++":
#         # "g++14":
#             Append: "-pg"

# Defines: ["NDEBUG=1"]

IncludePaths:
-   "../Include"
*/


#include "ncpp.n.hpp"
#include "Nstd/ReportError.n.hpp"
#include "Nstd/HeapAllocator.n.hpp"
#include "Nstd/AllocatorPool.n.hpp"
#include "Nstd/FastAllocator.n.hpp"
#include "Nstd/Filesystem.n.hpp"
#include "Nstd/StringUtil.n.hpp"

#include <stdio.h>

struct ClassInfo
{
    Nstd::String Name;
    Nstd::String Namespace;
};

struct FunctionInfo
{
    Nstd::String Name;
    Nstd::String Namespace;
    
    Nstd::String DeferFunc;
    
};


enum class ScopeType
{
    NONE,
    NAMESPACE,
    STRUCT,
    FUNC
};

n_view<const char> ScopeTypeToString(ScopeType st)
{
    switch(st)
    {
        case ScopeType::NONE:
            return "NONE";
        case ScopeType::NAMESPACE:
            return "NAMESPACE";
        case ScopeType::STRUCT:
            return "STRUCT";
        case ScopeType::FUNC:
            return "FUNC";
        default:
            return "";
    }
}

struct ScopeInfo //TODO: Add start scope index, maybe skip indices?... idk
{
    ScopeType Type;
    Nstd::String Name;
};

n_result<Nstd::String> FindIdentifier(  n_ref Nstd::Allocator& alloc,
                                        n_view<const char> statement, 
                                        n_view<const char> type, 
                                        n_ref bool& foundType)
{
    uint64 typeIndex = Nstd::ReverseFindSubString(statement, type, statement.len - 1);
    if(typeIndex == statement.len)
        return {};
    
    bool invalid =  typeIndex > 0 &&
                    (
                        Nstd::IsAlphaChar(statement[typeIndex - 1]) || 
                        Nstd::IsDigitChar(statement[typeIndex - 1])
                    );
    if(invalid)
        return {};
    
    foundType = true;
    //n_defer { statementStart = i + 1; };
    
    int j = typeIndex + type.len;
    for(; j < statement.len; ++j)
    {
        if(!Nstd::IsSpaceTabChar(statement.at<false>(j)))
            break;
    }
    
    if(j >= statement.len) //Can't find namespace identifier...
        return {};
    
    if(j == typeIndex + type.len) //At least 1 space
        return {};
    
    if(!Nstd::IsAlphaChar(statement.at<false>(j))) //Unexpected character for an identifier
        return {};
    
    Nstd::String name = name.Init(n_ref alloc, 32);
    for(; j < statement.len; ++j)
    {
        if( !Nstd::IsAlphaChar(statement.at<false>(j)) &&
            !Nstd::IsDigitChar(statement.at<false>(j)) &&
            statement.at<false>(j) != '_')
        {
            break;
        }
        name.Add(statement.at<false>(j)).n_try();
    }
    
    return name;
}

n_result<Nstd::String> TryParseFuncDecl(n_ref Nstd::Allocator& alloc, n_view<const char> statement)
{
    //TODO: Maybe consider template specialization?...
    //TODO: Consider default parameter value
    //TODO: ^ Only look for outer most brackets?
    
    uint64 openBracketIndex = Nstd::ReverseFindSubString(statement, "(", statement.len - 1);
    if(openBracketIndex == statement.len)
        return {};
    
    int identEndIndex = openBracketIndex;
    for(int j = (int)openBracketIndex - 1; j >= 0; --j)
    {
        if(!Nstd::IsSpaceTabChar(statement.at<false>(j)))
        {
            identEndIndex = j;
            break;
        }
    }
    
    if(identEndIndex == openBracketIndex)
        return {};
    
    int identStartIndex = identEndIndex;
    for(int j = identEndIndex; j >= 0; --j)
    {
        if( !Nstd::IsAlphaChar(statement.at<false>(j)) &&
            !Nstd::IsDigitChar(statement.at<false>(j)) &&
            statement.at<false>(j) != '_')
        {
            identStartIndex = j + 1;
            break;
        }
        
        if(j == 0)
        {
            identStartIndex = j;
            break;
        }
    }
    
    //There shouldn't be equal before ident, if it is a function declaration
    uint64 equalIndex = Nstd::ReverseFindSubString(statement, "=", identStartIndex);
    if(equalIndex != statement.len)
        return {};
    
    Nstd::String retString = 
        retString.InitString(   n_ref alloc, 
                                statement.sub(  identStartIndex, 
                                                identEndIndex - identStartIndex + 1));
    return retString;
}

bool InFunc(n_ref Nstd::List<ScopeInfo>& scopeInfos)
{
    for(int i = (int)scopeInfos.Len - 1; i >= 0; --i)
    {
        if(scopeInfos.Data[i].Type == ScopeType::FUNC)
            return true;
    }
    return false;
}

n_result<void> ParseSource(n_ref Nstd::Allocator& alloc, n_view<const char> source)
{
    //int curlyIndex = -1;
    int bracketIndex = -1;
    
    bool inString = false;
    bool inChar = false;
    bool inPreproc = false;
    bool inLineComment = false;
    bool inBlockComment = false;
    
    int statementStart = 0;
    //Nstd::List<Nstd::String> namespaces = namespaces.Init(n_ref alloc, 4);
    //Nstd::List<Nstd::String> classes = classes.Init(n_ref alloc, 4);
    
    
    Nstd::List<ScopeInfo> scopeInfos = scopeInfos.Init(n_ref alloc, 16);
    
    
    int debugLineNum = 1;
    (void)debugLineNum;
    
    n_result<void> addResult = {};
    
    #if 1
        #define SCOPE_PRINTF(...) printf(__VA_ARGS__)
    #else
        #define SCOPE_PRINTF(...)
    #endif
    
    #if 0
        #define DEBUG_PRINTF(...) printf(__VA_ARGS__)
    #else
        #define DEBUG_PRINTF(...)
    #endif
    
    for(int i = 0; i < source.len; ++i)
    {
        bool dontParse = inString || inChar || inPreproc || inLineComment || inBlockComment;
        addResult.n_try();
        
        switch(source.at<false>(i))
        {
            case '{':
            {
                if(dontParse)
                    break;
                
                ScopeType addType = ScopeType::NONE;
                Nstd::String identifer = {};
                
                n_defer 
                { 
                    addResult = scopeInfos.Add(ScopeInfo { addType, identifer }); 
                    if(addType != ScopeType::NONE)
                    {
                        n_view<const char> scopeStr = ScopeTypeToString(addType);
                        SCOPE_PRINTF(   "Added scope type %.*s with name %.*s on line %d\n", 
                                        (int)scopeStr.len,
                                        scopeStr.data,
                                        (int)identifer.Len(),
                                        identifer.Data(),
                                        debugLineNum);
                    }
                };
                
                if(InFunc(n_ref scopeInfos))
                    break;
                
                n_view<const char> statement = source.sub(statementStart, i - statementStart);
                bool foundNamespace = false;
                Nstd::String namespaceName = FindIdentifier(n_ref alloc, 
                                                            statement, 
                                                            "namespace", 
                                                            n_ref foundNamespace).n_try();
                statementStart = i + 1;
                if(foundNamespace)
                {
                    if(namespaceName.Len() != 0)
                    {
                        addType = ScopeType::NAMESPACE;
                        identifer = n_move(namespaceName);
                    }
                    break;
                }
                
                bool foundClass = false;
                Nstd::String className = FindIdentifier(n_ref alloc,
                                                        statement,
                                                        "class",
                                                        n_ref foundClass).n_try();
                if(foundClass)
                {
                    if(className.Len() != 0)
                    {
                        addType = ScopeType::STRUCT;
                        identifer = n_move(className);
                    }
                    break;
                }
                
                className = FindIdentifier(n_ref alloc, statement, "struct", n_ref foundClass).n_try();
                if(foundClass)
                {
                    if(className.Len() != 0)
                    {
                        addType = ScopeType::STRUCT;
                        identifer = n_move(className);
                    }
                    break;
                }
                
                Nstd::String funcName = TryParseFuncDecl(n_ref alloc, statement).n_try();
                if(funcName.Len())
                {
                    addType = ScopeType::FUNC;
                    identifer = n_move(funcName);
                }
                break;
            } //case '{':
            case '}':
                if(dontParse)
                    break;
                
                if(!scopeInfos.Len)
                    break;
                
                if(scopeInfos.At(scopeInfos.Len - 1).Type != ScopeType::NONE)
                {
                    ScopeInfo lastScopeInfo = scopeInfos.At(scopeInfos.Len - 1);
                    n_view<const char> scopeStr = ScopeTypeToString(lastScopeInfo.Type);
                    SCOPE_PRINTF(   "    Removed scope type %.*s with name %.*s on line %d\n", 
                                    (int)scopeStr.len,
                                    scopeStr.data,
                                    (int)lastScopeInfo.Name.Len(),
                                    lastScopeInfo.Name.Data(),
                                    debugLineNum);
                }
                
                scopeInfos.Remove(scopeInfos.Len - 1).n_try();
                
                //TODO(NOW): Parse stuff if we are in function
                
                break;
            case ';':
            {
                if(dontParse)
                    break;
                
                n_defer { statementStart = i + 1; };
                if(InFunc(n_ref scopeInfos))
                    break;
                
                n_view<const char> statement = source.sub(statementStart, i - statementStart);
                Nstd::String funcName = TryParseFuncDecl(n_ref alloc, statement).n_try();
                if(funcName.Len())
                {
                    n_view<const char> scopeStr = ScopeTypeToString(ScopeType::FUNC);
                    SCOPE_PRINTF(   "Added scope type %.*s with name %.*s on line %d\n", 
                                    (int)scopeStr.len,
                                    scopeStr.data,
                                    (int)funcName.Len(),
                                    funcName.Data(),
                                    debugLineNum);
                    scopeInfos.Add(ScopeInfo { ScopeType::FUNC, funcName }).n_try();
                }
                break;
            }
            case '#':
                if(dontParse)
                    break;
                inPreproc = true;
                DEBUG_PRINTF("    inPreproc on line %d\n", debugLineNum);
                break;
            case '"':
                if(!inString && dontParse)
                    break;
                
                if(inString && i > 0 && source.at(i - 1) == '\\') //Check \" when we are in string
                {
                    if(i <= 2)
                        break;
                    if(source.at<false>(i - 2) != '\\')
                        break;
                }
                
                //TODO: Check raw string
                inString = !inString;
                {
                    if(inString)
                        DEBUG_PRINTF("    inString on line %d\n", debugLineNum);
                    else
                        DEBUG_PRINTF("    off inString on line %d\n", debugLineNum);
                }
                break;
            case '\'':
                if(!inChar && dontParse)
                    break;
                
                if(inChar)
                {
                    if(i > 0 && source[i - 1] == '\\') //Check \' when we are in char
                    {
                        if(i <= 2)
                            break;
                        if(source.at<false>(i - 2) != '\\')
                            break;
                    }
                    inChar = false;
                    DEBUG_PRINTF("    off inChar on line %d\n", debugLineNum);
                }
                else
                {
                    inChar = true;
                    {
                        DEBUG_PRINTF("    inChar on line %d\n", debugLineNum);
                    }
                }
                break;
            case '(':
                if(dontParse)
                    break;
                ++bracketIndex;
                break;
            case ')':
                if(dontParse)
                    break;
                --bracketIndex;
                break;
            case '\n':
                if(inPreproc)
                {
                    if(i > 0 && source.at<false>(i - 1) != '\\')
                    {
                        inPreproc = false;
                        statementStart = i + 1;
                        DEBUG_PRINTF("    off inPreproc on line %d\n", debugLineNum);
                    }
                }
                else if(inLineComment)
                {
                    inLineComment = false;
                    DEBUG_PRINTF("    off inLineComment on line %d\n", debugLineNum);
                }
                
                ++debugLineNum;
                break;
            case '/':
                if(inBlockComment) //Check */
                {
                    if(i > 0 && source.at<false>(i - 1) == '*')
                    {
                        inBlockComment = false;
                        DEBUG_PRINTF("    off inBlockComment on line %d\n", debugLineNum);
                    }
                    break;
                }
                
                if(dontParse)
                    break;
                
                if(i > 0 && source.at<false>(i - 1) == '/')
                {
                    inLineComment = true;
                    DEBUG_PRINTF("    inLineComment on line %d\n", debugLineNum);
                }
                break;
            case '*':
                if(dontParse)
                    break;
                
                if(!inBlockComment && i > 0 && source.at<false>(i - 1) == '/') //Check /*
                {
                    inBlockComment = true;
                    DEBUG_PRINTF("    inBlockComment on line %d\n", debugLineNum);
                }
                break;
        
        } //switch(source.at<false>(i))
    } //for(int i = 0; i < source.len; ++i)
    
    n_check_eq(scopeInfos.Len, 0);
    n_check_eq(bracketIndex, -1);
    n_check_false(inString);
    
    return {};
}



n_result<int> Main(int argc, char** argv)
{
    if(argc < 2 || n_view<const char>(argv[1]) == "--help" || n_view<const char>(argv[1]) == "help")
    {
        printf("%s [--include <dir>]... --source <file>[...]\n", argv[0]);
        return 0;
    }
    
    Nstd::HeapAllocator h = h.Init(1024).n_try();
    Nstd::AllocatorPool<Nstd::FastAllocator<>> allocatorPool = allocatorPool.Init(  h.MakeAllocator(), 
                                                                                    1024).n_try();
    Nstd::Allocator alloc = allocatorPool.MakeAllocator();
    
    Nstd::List<n_view<const char>> includes = includes.Init(n_ref alloc, argc);
    Nstd::List<n_view<const char>> sources = sources.Init(n_ref alloc, argc);
    
    for(int i = 1; i < argc; ++i)
    {
        if(n_view<const char>(argv[i]) == "--include")
        {
            if(i == argc - 1)
                return n_error_msg("Missing arg value for --include");
            includes.Add(argv[++i]).n_try();
        }
        else if(n_view<const char>(argv[i]) == "--source")
        {
            if(i == argc - 1)
                return n_error_msg("Missing arg value for --source");
            sources.Add(argv[++i]).n_try();
        }
        else
            return n_error_msg("Unexpected argument %s", argv[i]);
    }
    
    for(int i = 0; i < sources.Len; ++i)
    {
        Nstd::List<uint8> sourceBinary = Nstd::FileReadAll(n_ref alloc, sources.At(i)).n_try();
        n_defer { sourceBinary.Free(); };
        n_view<const char> sourceView = sourceBinary.ToView().as<const char>();
        ParseSource(n_ref alloc, sourceView).n_try();
    }
    
    return 0;
}

int main(int argc, char** argv)
{
    int r = Main(argc, argv).n_try_act( printf("FAILED.\n");
                                        NSTD_REPORT_ERROR();
                                        return 1);
    return r;
}
