local cl = require("ljclang")
local vr = cl.ChildVisitResult
local ffi = require("ffi")

local ret = {}

ret.fail = false

local function print_fancy(color, str, c)
  io.stderr:write(color)
  if c then
    local file, line = c:location()
    io.stderr:write(file, ":", line, ": ", tostring(c), ": ")
  end
  io.stderr:write(str, "\27[0m\n")
end

function ret.print_error(str, c)
  ret.fail = true
  print_fancy("\27[31m", str, c)
end

function ret.print_warning(str, c)
  print_fancy("\27[33m", str, c)
end

function ret.default_name_class(c)
  return c:name():gsub("<.*", ""): -- eat template arguments
    gsub("(%l)(%u)", "%1_%2"):gsub("::", "."):lower()
end

function ret.default_name_fun(c)
  if c:kind() == "Constructor" then return "new" end
  return c:name():gsub("(%l)(%u)", "%1_%2"):lower()
end

function ret.fun_qualified_name(c)
  local tbl = { c:name() }
  local x = c:parent()
  local i = 1
  while x and x:kind() == "Namespace" do
    i = i-1
    tbl[i] = x:name()
    x = x:parent()
  end
  return table.concat(tbl, "::", i, 1)
end

function ret.default_arg(tbl, arg, def_fun, ...)
  if tbl[arg] == nil then
    tbl[arg] = def_fun(...)
  end
end

function ret.add_alias(aliases, type, alias)
  aliases[type:name():gsub("[.%[%]*+%-?%%^$]", "%%%0")] = alias:gsub("%%", "%%%%")
end

-- extremely hacky way to get fully qualified-ish names
-- totally not optimal, but this is probably the best we can get without
-- hacking even more clang internals or writing a real libtooling tool in C++...

-- collect fully qualified names of the type and all template arguments and all
-- base types
--
-- the latter is required in case of templates:
-- void foo(const typename Bar<T, Asd>::InnerStuff&)
-- we need to turn Bar<...>::InnerStuff into Neptools::Bar...
-- but we only get the InnerStuff type, it's path won't match the template hell
-- calling :canonical() on it will resolve the typedef so it won't work
--
-- tbl: to be filled by this function
--   key=fqn, value=path, a table: {start=integer, [start]="pathitem"...[1]="class name"}
-- t: the type
local collect_ns_cur
local function collect_ns(tbl, t)
  if not t then return end

  local p = t:pointee()
  if p then collect_ns(tbl, p) end

  local cur = t:declaration()
  if cur:kind() == "NoDeclFound" then return end

  for k,v in ipairs(t:templateArguments()) do collect_ns(tbl, v) end
  collect_ns_cur(tbl, cur)
end

collect_ns_cur = function(tbl, cur)
  local path = {cur:name()}
  local repl = {cur:name()}
  local i = 0
  local par = cur:parent()
  while par and par:kind() ~= "TranslationUnit" do
    collect_ns(tbl, par:type())
    path[i] = par:name()
    repl[i] = par:displayName()
    i=i-1
    par = par:parent()
  end

  path.start = i+1
  tbl["::"..table.concat(repl, "::", i+1, 1)] = path
end

-- generate gsub patterns for collected paths
local function gen_gsub(tbl)
  local pats = {}
  --print() print("===============================start==========================")

  for k,v in pairs(tbl) do
    local repl = "%1"..k.."%2"
    --print(k, v, repl)

    for i=v.start,1 do
      local pat = "([^a-zA-Z:])"..table.concat(v, "::", i, 1).."([^a-zA-Z0-9_])"
      --print(pat)
      assert(not pats[pat] or pats[pat] == repl, "Ambiguous name?")
      pats[pat] = repl
    end
    --print()
  end

  return pats
end

local find_typedefs_tbl
local find_typedefs_v = cl.regCursorVisitor(function (c, par)
  local kind = c:kind()
  if kind == "TypeAliasDecl" then
    collect_ns_cur(find_typedefs_tbl, c)
  --else print("Unhandled", kind, c:name())
  end
  return vr.Continue
end)
local function find_typedefs(tbl, cur)
  if cur == nil then return end
  find_typedefs_tbl = tbl
  cur:children(find_typedefs_v)
  return find_typedefs(tbl, cur:parent())
end

local function get_type_intname(x, cur)
  local t
  if type(x) == "string" then return x
  elseif ffi.istype(cl.Cursor_t, x) then
    t = x:type()
    assert(not cur)
    cur = x
  elseif ffi.istype(cl.Type_t, x) then t = x
  else error("invalid type parameter") end

  local tbl = {}
  collect_ns(tbl, t)
  if cur then find_typedefs(tbl, cur, t:name()) end
  local pats = gen_gsub(tbl)

  local ret = "#"..t:name().."#"
  --print("\n\nstart", ret)
  local chg = true
  while chg do
    chg = false
    for k,v in pairs(pats) do
      --print(k,"----",v)
      local n = ret:gsub(k, v)
      if n ~= ret then
        ret = n
        chg = true
      end
    end
  end

  --print("return", ret)
  return ret:sub(2, -2)
end

local function type_name(typ, aliases, cur)
  local n = get_type_intname(typ, cur)
  for k,v in pairs(aliases) do
    n = n:gsub(k, v)
    --print(n, k, v)
  end
  return n:gsub("std::__cxx11::", "std::"):gsub("std::__1::", "std::")
end
ret.type_name = type_name

function ret.type_list(args, aliases, wrap, pre)
  if wrap then
    for i,v in ipairs(args) do
      args[i] = wrap.."<"..type_name(args[i], aliases)..">"
    end
  else
    for i,v in ipairs(args) do
      args[i] = type_name(args[i], aliases)
    end
  end
  local cat = table.concat(args, ", ")
  if pre and cat ~= "" then return ", "..cat end
  return cat
end

function ret.is_lua_annotation(c)
  if c:name():sub(1,4) == "lua{" then
    return assert(loadstring("return "..c:name():sub(4)))()
  end
end

-- get annotation
local get_annot_tbl
local get_annot_v = cl.regCursorVisitor(function (c, par)
  if c:kind() == "AnnotateAttr" then
    local x = ret.is_lua_annotation(c)
    if x then get_annot_tbl[#get_annot_tbl+1] = x end
  end
  return vr.Continue
end)

function ret.get_annotations(c)
  get_annot_tbl = {}
  c:children(get_annot_v)
  return get_annot_tbl
end

ffi.cdef("char* getcwd(char* buf, size_t size)")
function ret.getcwd()
  local siz = 64
  while true do
    local dat = ffi.new("char[?]", siz)
    local ok = ffi.C.getcwd(dat, siz)
    if ok == dat then return ffi.string(dat) end
    if ffi.errno() ~= 34 then error("getcwd failed") end
    siz = siz*2
  end
end

function ret.parse_path(base, path)
  if not path then return {} end
  if path:sub(1,1) ~= '/' then path = base..'/'..path end
  local ret = {}
  for c in path:gmatch("[^/]+") do
    if c == ".." then
      assert(ret[#ret])
      ret[#ret] = nil
    elseif c ~= "." then
      ret[#ret+1] = c
    end
  end
  return ret
end

return ret
