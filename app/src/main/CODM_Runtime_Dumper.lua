gg.setVisible(false)
gg.alert([[
  [+] Telegram Channel : https://t.me/retiredgamermods
  [+] Telegram Group   : https://t.me/magicmodschat
  [+] YouTube          : https://youtube.com/@magicmods-u5k?si=rQohKMMV225WO0yW

  CODM Runtime Dumper - Dumps libunity + global-metadata.dat
]], "[ OK ]")

local gg = gg

function toHex(val)
  return string.format('%x', val)
end

function deleteMapFile(path, packageName)
  local mapFile = path..'/'..packageName..'-maps.txt'
  os.remove(mapFile)
end

function renameFile(starting, ending, pathing, naming, packageName, fileType)
  local oldPath = pathing..'/'..packageName..'-'..toHex(starting)..'-'..toHex(ending)..'.bin'
  local newPath = pathing..'/'..naming
  os.rename(oldPath, newPath)

  deleteMapFile(pathing, packageName)

  local title = fileType.." Dump Complete!"
  local pathText = "File saved to: "..newPath
  local border = "======================="

  gg.setVisible(true)

  print("\n"..border)
  print("📌 "..title)
  print(border)
  print("[ 📂 ] ➣ "..newPath)

  gg.toast(title.."\n"..pathText)
end

function getEnd(lib)
  local t = gg.getRangesList(lib)
  return t[#t]['end'] - 1
end

function dumpLib(index, path)
  local starting = lib_t[index].start
  local ending = lib_t[index].endForDump
  local naming = toHex(starting)..'_'..names[index]

  gg.setVisible(false)
  gg.dumpMemory(starting, ending, path)
  renameFile(starting, ending + 1, path, naming, info.packageName, "libunity")
end

function autoDump()
  local path = "/storage/emulated/0/Download"
  local index = nil
  for i, v in ipairs(names) do
    if v == "libunity.so" then
      index = i
      break
    end
  end
  if not index then return print("libunity.so not found in the game's process.") end
  dumpLib(index, path)
end

function dumpMetadata()
  local path = "/storage/emulated/0/Download"
  local t = gg.getRangesList('global-metadata.dat')
  if not t[1] then return print('This game \"'..info.label..'\" does not have global-metadata.dat') end
  local starting = t[1].start
  local ending = t[1]['end']

  gg.setVisible(false)
  gg.dumpMemory(starting, ending - 1, path)
  renameFile(starting, ending, path, 'global-metadata.dat', info.packageName, "Metadata")
end

lib_t = {}
names = {}
info = gg.getTargetInfo()

for i, v in ipairs(gg.getRangesList('/data/*.so')) do
  if not string.find(v.internalName, info.packageName) then goto skipLib end
  v.shortName = string.match(v.internalName, '[^/]+$'):gsub(':.*', '')
  for _, b in ipairs(lib_t) do
    if b.shortName == v.shortName then goto skipLib end
  end
  v.endForDump = getEnd(v.shortName)
  table.insert(lib_t, v)
  table.insert(names, v.shortName)
  ::skipLib::
end

if #names == 0 then
  return print('What Are You Doing..🤦 \"'..info.label..'\" 🫡 Are You Noob Brohhh ?')
end

autoDump()
dumpMetadata()