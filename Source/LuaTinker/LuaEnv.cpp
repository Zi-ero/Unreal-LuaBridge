// Fill out your copyright notice in the Description page of Project Settings.


#include "LuaEnv.h"
#include "MyActor.h"
#include "Kismet/KismetSystemLibrary.h"

int print_lua_stack(lua_State* L) {
    int top = lua_gettop(L); // ��ȡ��ջ��������
    
    for (int i = 1; i <= top; i++) {
        int type = lua_type(L, i); // ��ȡֵ������
        UE_LOG(LogTemp, Error, TEXT("print_lua_stack: %d"), type);
    }

    UE_LOG(LogTemp, Error, TEXT("print_lua_stack Size: %d"), lua_gettop(L));

    return 0;
}


void PushUPropertyToLua(lua_State* L, FProperty* Property, UObject* Obj)
{
    if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
    {
        float Value = FloatProperty->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<float>(Obj));
        lua_pushnumber(L, Value);
    }
    else if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
    {
        float Value = DoubleProperty->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<double>(Obj));
        lua_pushnumber(L, Value);
    }
    else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
    {
        int Value = IntProperty->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<int>(Obj));
        lua_pushnumber(L, Value);
    }
    else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
    {
        bool Value = BoolProperty->GetPropertyValue(Property->ContainerPtrToValuePtr<void>(Obj));
        lua_pushboolean(L, Value);
    }
    else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
    {
        UObject* Value = ObjectProperty->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(Obj));
        LuaEnv::PushUserData(Value);
    }
    else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
    {
        FString Value = StringProperty->GetPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(Obj));
        lua_pushstring(L, TCHAR_TO_UTF8(*Value));
    }
}

void PushBytesToLua(lua_State* L, FProperty* Property, BYTE* Params)
{
    if (Property == NULL || Params == NULL)
    {
        return;
    }
    else if (Property->IsA<FFloatProperty>())
    {
        float Value = *(float*)Params;
        lua_pushnumber(L, Value);
    }
    else if (Property->IsA<FDoubleProperty>())
    {
        double Value = *(double*)Params;
        lua_pushnumber(L, Value);
    }
    else if (Property->IsA<FIntProperty>())
    {
        int Value = *(int*)Params;
        lua_pushnumber(L, Value);
    }
    else if (Property->IsA<FBoolProperty>())
    {
        bool Value = *(bool*)Params;
        lua_pushboolean(L, Value);
    }
    else if (Property->IsA<FObjectProperty>())
    {
        UObject* Value = *(UObject**)Params;
        LuaEnv::PushUserData(Value);
    }
    else if (Property->IsA<FStrProperty>())
    {
        FString Value = *(FString*)Params;
        lua_pushstring(L, TCHAR_TO_UTF8(*Value));
    }
}

// luaջ�Ĳ���ѹ����ã����Ҹ�������ֵ����
FProperty* PrepareParms(lua_State* L, UFunction* Func, BYTE* Params)
{
    int LuaParamNum = lua_gettop(L);
    int Offset = 0;
    int i = 1;

    if (Func == NULL || Params == NULL)
    {
        return NULL;
    }

    for (TFieldIterator<FProperty> IteratorOfParam(Func); IteratorOfParam; ++IteratorOfParam) {
        FProperty* Param = *IteratorOfParam;
        Offset = Param->GetOffset_ForInternal();

        if (Offset >= Func->ReturnValueOffset)
        {
            lua_settop(L, 0);
            return Param;
        }

        if (i > LuaParamNum)
        {
            break;
        }

        if (Param->IsA<FFloatProperty>())
        {
            *(float*)(Params + Offset) = (float)lua_tonumber(L, i);
        }
        else if (Param->IsA<FDoubleProperty>() != NULL)
        {
            *(double*)(Params + Offset) = (double)lua_tonumber(L, i);
        }
        else if (Param->IsA<FIntProperty>() != NULL)
        {
            *(int*)(Params + Offset) = (int)lua_tonumber(L, i);
        }
        else if (Param->IsA<FBoolProperty>() != NULL)
        {
            *(bool*)(Params + Offset) = (bool)lua_toboolean(L, i);
        }
        else if (Param->IsA<FObjectProperty>() != NULL)
        {
            *(UObject**)(Params + Offset) = (UObject*)lua_touserdata(L, i);
        }
        else if (Param->IsA<FStrProperty>() != NULL)
        {
            new(Params + Offset)FString(lua_tostring(L, i));
        }

        i++;
    }

    lua_settop(L, 0);

    return NULL;
}

int LuaCallUFunction(lua_State* L)
{
    UObject*   Obj  = (UObject*)lua_touserdata(L, lua_upvalueindex(1));
    UFunction* Func = (UFunction*)lua_touserdata(L, lua_upvalueindex(2));

    BYTE* Parms = (BYTE*)FMemory::Malloc(Func->ParmsSize);
    FMemory::Memzero(Parms, Func->ParmsSize);
    FProperty* RetType = PrepareParms(L, Func, Parms);
    Obj->ProcessEvent(Func, Parms); // ���ú���
    PushBytesToLua(L, RetType, Parms + Func->ReturnValueOffset); // ѹ�ط���ֵ
    FMemory::Free(Parms); // �ͷŲ����ڴ�

    return RetType == NULL ? 0 : 1;
}

void PushUFunctionToLua(lua_State* L, UFunction* Func, UObject* Obj)
{
    lua_pushlightuserdata(L, Obj);
    lua_pushlightuserdata(L, Func);
    lua_pushcclosure(L, LuaCallUFunction, 2);
}

/**
 * __index meta methods for class
 */
int Class_Index(lua_State* L)
{
    // ��ȡ Lua �����
    luaL_checktype(L, -2, LUA_TTABLE);

    // ��ȡ����
    const char* key = lua_tostring(L, -1);
    lua_pop(L, 1);

    // ���Դ�UObject����ֵ
    // �ö�Ӧ��UObjectָ��
    lua_pushstring(L, "NativePtr");
    lua_rawget(L, -2);
    if (lua_islightuserdata(L, -1))
    {
        UObject* Obj = (UObject*)lua_touserdata(L, -1);
        lua_pop(L, 1);
        // ����ӦUProperty����ֵѹջ
        FName PropertyName(key);
        for (TFieldIterator<FProperty> Property(Obj->GetClass()); Property; ++Property)
        {
            if (Property->GetFName() == PropertyName)
            {
                PushUPropertyToLua(L, *Property, Obj); // �ٰ�ֵ����ȥ
                return 1;
            }
        }

        UFunction* UFunc = Obj->FindFunction(PropertyName);
        if (UFunc)
        {
            PushUFunctionToLua(L, UFunc, Obj);
            return 1;
        }
    }
    else
    {
        lua_pop(L, 1); // ����nil
    }

    lua_pushstring(L, "__ClassDesc");
    lua_rawget(L, -2);
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, key);
    lua_gettable(L, -2);
    lua_remove(L, -2); // ��__ClassDesc�õ��ı��Ƴ�ȥ

    return 1;
}

/**
 * __newindex meta methods for class
 */
int Class_NewIndex(lua_State* L)
{
    // ��ȡ Lua �����
    luaL_checktype(L, 1, LUA_TTABLE);

    // ��ȡ������ֵ
    const char* key = luaL_checkstring(L, 2);
    int value = luaL_checkinteger(L, 3);

    // �ڴ˴�����ʵ���Զ���ĸ�ֵ��Ϊ�����������ض��ļ���Ӧ��ֵ

    return 0;  // ����ֵ����Ϊ0
}

/**
 * Generic closure to call a UFunction
 */
int Class_CallUFunction(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    // ��ȡ���ò���
    int arg1 = luaL_checkinteger(L, 2);
    int arg2 = luaL_checkinteger(L, 3);

    // ����һЩ����
    int sum = arg1 + arg2;

    // �����ѹ�� Lua ջ
    lua_pushinteger(L, sum);

    return 1;  // ����ֵ����Ϊ1
}

static void CreateUnrealMetaTable(lua_State* L)
{
    int Type = luaL_getmetatable(L, "__UMT");
    if (Type == LUA_TTABLE)
    {
        return;
    }

    // û�еĻ��Ȱ�nil������Ȼ�󴴽�һ��metatable
    lua_pop(L, 1);
    luaL_newmetatable(L, "__UMT");
    lua_pushvalue(L, -1);
    lua_setmetatable(L, -2);

    //lua_pushstring(L, "__index");
    //lua_pushcfunction(L, Class_Index);
    //lua_rawset(L, -3);

    //lua_pushstring(L, "__call");
    //lua_pushcfunction(L, Class_CallUFunction);
    //lua_rawset(L, -3);
}

static int CreateUClass(lua_State* L)
{
    lua_createtable(L, 0, 0);
    lua_createtable(L, 0, 0);
    lua_pushstring(L, "__index");
    CreateUnrealMetaTable(L);
    lua_rawset(L, -3);
    lua_setmetatable(L, -2);
    return 1;
}

static FString GetMessage(lua_State* L)
{
    const auto ArgCount = lua_gettop(L);
    FString Message;
    if (!lua_checkstack(L, ArgCount))
    {
        luaL_error(L, "too many arguments, stack overflow");
        return Message;
    }
    for (int ArgIndex = 1; ArgIndex <= ArgCount; ArgIndex++)
    {
        if (ArgIndex > 1)
            Message += "\t";
        Message += UTF8_TO_TCHAR(luaL_tolstring(L, ArgIndex, NULL));
    }
    return Message;
}

static int LogInfo(lua_State* L)
{
    const auto Msg = GetMessage(L);
    UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
    return 0;
}

static int ErrorInfo(lua_State* L)
{
    const auto Msg = GetMessage(L);
    UE_LOG(LogTemp, Error, TEXT("%s"), *Msg);
    return 0;
}

static const struct luaL_Reg UE_Base_Lib[] = {
  {"UClass", CreateUClass},
  {"log", LogInfo},
  {"error", ErrorInfo},
  {"dump", print_lua_stack},
  {NULL, NULL}  /* sentinel */
};

int luaopen_UE_BaseLib(lua_State* L) {
    luaL_newlib(L, UE_Base_Lib);
    return 1;
}

LuaEnv::LuaEnv()
{
    if (L == nullptr)
    {
        L = luaL_newstate();
        luaL_openlibs(L);
        luaL_requiref(L, "UE", luaopen_UE_BaseLib, 1);
    }
}

LuaEnv::~LuaEnv()
{
    lua_close(L);
    L = nullptr;
}

bool LuaEnv::TryToBind(UObject* Object)
{
    if (!IsValid(Object))           // filter out CDO and ArchetypeObjects
    {
        return false;
    }

    UClass* Class = Object->GetClass();
    if (Class->IsChildOf<UPackage>() || Class->IsChildOf<UClass>())             // filter out UPackage and UClass
    {
        return false;
    }

    if (Object->IsDefaultSubobject())
    {
        return false;
    }

    UFunction* Func = Class->FindFunctionByName(FName("GetModuleName"));    // find UFunction 'GetModuleName'. hard coded!!!
    if (Func)
    {
        if (Func->GetNativeFunc() && IsInGameThread())
        {
            FString ModuleName;
            UObject* DefaultObject = Class->GetDefaultObject();             // get CDO
            DefaultObject->UObject::ProcessEvent(Func, &ModuleName);        // force to invoke UObject::ProcessEvent(...)
            
            UE_LOG(LogTemp, Error, TEXT("ProcessEvent ModuleName: %s"), *ModuleName);
            
            if (ModuleName.Len() < 1)
            {
                ModuleName = Class->GetName();
            }
            return BindInternal(Object, Class, *ModuleName);   // bind!!!
        }
        else
        {
            return false;
        }
    }

    return false;
}

bool LuaEnv::BindInternal(UObject* Object, UClass* Class, const TCHAR* InModuleName)
{
    check(Object);

    UE_LOG(LogTemp, Error, TEXT("InModuleName: %s"), InModuleName);

    // functions
    for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::ExcludeInterfaces); It; ++It)
    {
        if (It->GetName() == FString("TestFunc"))
        {
            It->SetNativeFunc(&LuaEnv::execCallLua);
        }
    }

    return BindTableForObject(Object, TCHAR_TO_UTF8(InModuleName));
}


DEFINE_FUNCTION(LuaEnv::execCallLua)
{
    UFunction* NativeFunc = Stack.CurrentNativeFunction;
    P_FINISH;

    UE_LOG(LogTemp, Error, TEXT("execCallLua: Obj %s, Native Func %s"), *Context->GetName(), *NativeFunc->GetName());

    const char* ModuleName = TCHAR_TO_UTF8(*Context->GetClass()->GetName());
    if (lua_getglobal(L, ModuleName) != LUA_TTABLE)
    {
        lua_pop(L, 1);
        return;
    }

    lua_pushstring(L, TCHAR_TO_UTF8(*NativeFunc->GetName()));
    lua_gettable(L, -2); // �ڱ��в��Ҷ�Ӧ����
    if (lua_isfunction(L, -1))
    {
        PushUserData(Context); // ��UObject��ָ����ΪSelf����
        lua_pcall(L, 1, 1, 0);
        lua_Number LuaRet = lua_tonumber(L, -1);
        lua_pop(L, 1);
        UE_LOG(LogTemp, Error, TEXT("Lua Ret %f"), LuaRet);
    }
    else
    {
        lua_pop(L, 1); // ����nil
    }

    lua_pop(L, 1); // ����Table
}

void LuaEnv::NotifyUObjectCreated(const UObjectBase* ObjectBase, int Index)
{
    static UClass* InterfaceClass = AMyActor::StaticClass();
    if (!ObjectBase->GetClass()->IsChildOf<AMyActor>())
    {
        return;
    }

    TryToBind((UObject*)(ObjectBase));
}

void LuaEnv::PushUserData(UObject* Object)
{
    lua_createtable(L, 0, 0);
    lua_pushstring(L, "NativePtr");
    lua_pushlightuserdata(L, Object);
    lua_rawset(L, -3);

    lua_pushstring(L, "__index");
    lua_pushcfunction(L, Class_Index);
    lua_rawset(L, -3);

    lua_pushvalue(L, -1);
    lua_setmetatable(L, -2);

    lua_pushstring(L, "__ClassDesc");
    int Type = lua_getglobal(L, TCHAR_TO_UTF8(*Object->GetClass()->GetName()));
    if (Type == LUA_TTABLE)
    {
        lua_rawset(L, -3);
    }
    else
    {
        lua_pop(L, 2);
    }
}

bool LuaEnv::BindTableForObject(UObject* Object, const char* InModuleName)
{
    //lua_getglobal(L, "require");
    FString ProjectDir = UKismetSystemLibrary::GetProjectDirectory();
    ProjectDir += InModuleName;
    ProjectDir += ".lua";
    //lua_pushstring(L, TCHAR_TO_UTF8(*ProjectDir));

    // ����require�����������ַ���������require����������ջ��
    //if (lua_pcall(L, 1, 1, 0) != LUA_OK)
    //{
    //    return false;
    //}

    luaL_dofile(L, TCHAR_TO_UTF8(*ProjectDir));

    // ���سɹ�������ֵΪ��
    if (!lua_istable(L, -1))
    {
        return false;
    }

    lua_setglobal(L, InModuleName);
    lua_pop(L, 1);

    return true;
}
