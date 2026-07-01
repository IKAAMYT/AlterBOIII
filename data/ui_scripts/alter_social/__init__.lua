-- AlterBO3 (IKAAM) — Menu Social custom (version stylisee)
--
-- Remplace le menu social natif "Social_Main" (qui crashe : services Live morts)
-- par notre propre menu LUI. Lit la liste d'amis via les fonctions natives du
-- client (game.getfriendcount / game.getfriend / game.connecttofriend).

if Engine.GetCurrentMap() ~= "core_frontend" then
  return
end

if not LUI or not LUI.createMenu or not CoD or not CoD.Menu then
  return
end

local ACCENT = { 0.949, 0.769, 0.067 }
local COL_TEXT = { 0.96, 0.95, 0.93 }
local COL_DIM = { 0.62, 0.60, 0.55 }
local FONT_TITLE = "fonts/RefrigeratorDeluxe-Regular.ttf"
local FONT_BODY = "fonts/default.ttf"

local function stateInfo(state)
  if state == 2 then
    return "En partie", { 0.35, 0.85, 0.4 }
  elseif state == 1 then
    return "En ligne", { 0.4, 0.75, 1.0 }
  end
  return "Hors ligne", { 0.45, 0.45, 0.48 }
end

local function safeFont(element, font)
  pcall(function()
    element:setTTF(font)
  end)
end

local function makeFriendCard(parent, controller, friend, top)
  local card = LUI.UIElement.new()
  card:setLeftRight(true, false, 60, 720)
  card:setTopBottom(true, false, top, top + 56)
  card:makeFocusable()
  card:setHandleMouse(true)

  local bg = LUI.UIImage.new()
  bg:setLeftRight(true, true, 0, 0)
  bg:setTopBottom(true, true, 0, 0)
  bg:setRGB(0.10, 0.095, 0.08)
  bg:setAlpha(0.85)
  card:addElement(bg)

  local accentBar = LUI.UIImage.new()
  accentBar:setLeftRight(true, false, 0, 4)
  accentBar:setTopBottom(true, true, 0, 0)
  accentBar:setRGB(ACCENT[1], ACCENT[2], ACCENT[3])
  accentBar:setAlpha(0.9)
  card:addElement(accentBar)

  local label, dotColor = stateInfo(friend.status)

  local dot = LUI.UIImage.new()
  dot:setLeftRight(true, false, 22, 34)
  dot:setTopBottom(true, false, 22, 34)
  dot:setRGB(dotColor[1], dotColor[2], dotColor[3])
  card:addElement(dot)

  local nameText = LUI.UIText.new()
  nameText:setLeftRight(true, false, 52, 460)
  nameText:setTopBottom(true, false, 10, 36)
  nameText:setText(friend.name or "?")
  safeFont(nameText, FONT_BODY)
  nameText:setRGB(COL_TEXT[1], COL_TEXT[2], COL_TEXT[3])
  nameText:setAlignment(LUI.Alignment.Left)
  card:addElement(nameText)

  local stateText = LUI.UIText.new()
  stateText:setLeftRight(true, false, 480, 640)
  stateText:setTopBottom(true, false, 14, 36)
  stateText:setText(label)
  safeFont(stateText, FONT_BODY)
  stateText:setRGB(dotColor[1], dotColor[2], dotColor[3])
  stateText:setAlignment(LUI.Alignment.Left)
  card:addElement(stateText)

  if friend.status == 2 then
    local joinHint = LUI.UIText.new()
    joinHint:setLeftRight(false, true, -180, -16)
    joinHint:setTopBottom(true, false, 16, 36)
    joinHint:setText("Cliquer pour rejoindre")
    safeFont(joinHint, FONT_BODY)
    joinHint:setRGB(ACCENT[1], ACCENT[2], ACCENT[3])
    joinHint:setAlignment(LUI.Alignment.Right)
    card:addElement(joinHint)
  end

  card:registerEventHandler("mouseenter", function()
    bg:setRGB(0.16, 0.15, 0.12)
  end)
  card:registerEventHandler("mouseleave", function()
    bg:setRGB(0.10, 0.095, 0.08)
  end)

  if CoD.isPC then
    card:registerEventHandler("leftmousedown", function()
      if friend.status == 2 and friend.steam_id and friend.steam_id ~= "0" then
        pcall(function()
          game.connecttofriend(friend.steam_id)
        end)
      end
    end)
  end

  parent:addElement(card)
  return card
end

LUI.createMenu.AlterSocialMenu = function(controller)
  local self = CoD.Menu.NewForUIEditor("AlterSocialMenu")
  if PreLoadFunc then
    PreLoadFunc(self, controller)
  end
  self:setOwner(controller)
  self:setLeftRight(true, true, 0, 0)
  self:setTopBottom(true, true, 0, 0)
  self.anyChildUsesUpdateState = true

  local backdrop = LUI.UIImage.new()
  backdrop:setLeftRight(true, true, 0, 0)
  backdrop:setTopBottom(true, true, 0, 0)
  backdrop:setRGB(0.03, 0.03, 0.04)
  backdrop:setAlpha(0.97)
  self:addElement(backdrop)

  local header = LUI.UIImage.new()
  header:setLeftRight(true, true, 0, 0)
  header:setTopBottom(true, false, 0, 96)
  header:setRGB(0.07, 0.065, 0.05)
  header:setAlpha(0.95)
  self:addElement(header)

  local headerLine = LUI.UIImage.new()
  headerLine:setLeftRight(true, true, 0, 0)
  headerLine:setTopBottom(true, false, 94, 96)
  headerLine:setRGB(ACCENT[1], ACCENT[2], ACCENT[3])
  headerLine:setAlpha(0.85)
  self:addElement(headerLine)

  local title = LUI.UIText.new()
  title:setLeftRight(true, false, 60, 700)
  title:setTopBottom(true, false, 30, 74)
  title:setText("AMIS")
  safeFont(title, FONT_TITLE)
  title:setRGB(ACCENT[1], ACCENT[2], ACCENT[3])
  title:setAlignment(LUI.Alignment.Left)
  self:addElement(title)

  local subtitle = LUI.UIText.new()
  subtitle:setLeftRight(false, true, -320, -60)
  subtitle:setTopBottom(true, false, 42, 66)
  subtitle:setText("AlterCOD")
  safeFont(subtitle, FONT_BODY)
  subtitle:setRGB(COL_DIM[1], COL_DIM[2], COL_DIM[3])
  subtitle:setAlignment(LUI.Alignment.Right)
  self:addElement(subtitle)

  local count = 0
  pcall(function()
    count = game.getfriendcount() or 0
  end)

  local recap = LUI.UIText.new()
  recap:setLeftRight(true, false, 60, 400)
  recap:setTopBottom(true, false, 112, 138)
  recap:setText(count .. (count > 1 and " amis" or " ami"))
  safeFont(recap, FONT_BODY)
  recap:setRGB(COL_DIM[1], COL_DIM[2], COL_DIM[3])
  recap:setAlignment(LUI.Alignment.Left)
  self:addElement(recap)

  if count == 0 then
    local empty = LUI.UIText.new()
    empty:setLeftRight(true, false, 60, 720)
    empty:setTopBottom(true, false, 200, 240)
    empty:setText("Aucun ami pour le moment.")
    safeFont(empty, FONT_BODY)
    empty:setRGB(COL_TEXT[1], COL_TEXT[2], COL_TEXT[3])
    empty:setAlignment(LUI.Alignment.Left)
    self:addElement(empty)

    local hint = LUI.UIText.new()
    hint:setLeftRight(true, false, 60, 720)
    hint:setTopBottom(true, false, 236, 270)
    hint:setText("Ajoute des amis depuis le launcher AlterBO3.")
    safeFont(hint, FONT_BODY)
    hint:setRGB(COL_DIM[1], COL_DIM[2], COL_DIM[3])
    hint:setAlignment(LUI.Alignment.Left)
    self:addElement(hint)
  else
    local top = 150
    for i = 0, count - 1 do
      local friend = nil
      pcall(function()
        friend = game.getfriend(i)
      end)
      if friend then
        makeFriendCard(self, controller, friend, top)
        top = top + 64
      end
    end
  end

  self:AddButtonCallbackFunction(
    self,
    controller,
    Enum.LUIButton.LUI_KEY_XBB_PSCIRCLE,
    nil,
    function(element, menu, controller, model)
      GoBack(self, controller)
      return true
    end,
    function(element, menu, controller)
      CoD.Menu.SetButtonLabel(menu, Enum.LUIButton.LUI_KEY_XBB_PSCIRCLE, "MENU_BACK")
      return true
    end,
    false
  )

  self:processEvent({ name = "menu_loaded", controller = controller })
  self:processEvent({ name = "update_state", menu = self })

  return self
end

local function resolveController(a, b)
  if type(a) == "number" then
    return a
  end
  if type(b) == "number" then
    return b
  end
  if Engine and Engine.GetFirstActiveController then
    return Engine.GetFirstActiveController()
  end
  return 0
end

if type(OpenPopup) == "function" then
  local originalOpenPopup = OpenPopup
  OpenPopup = function(menu, menuName, controller, ...)
    if menuName == "Social_Main" then
      local ctrl = resolveController(controller, menu)
      return LUI.FlowManager.RequestAddMenu(ctrl, "AlterSocialMenu", true, nil)
    end
    return originalOpenPopup(menu, menuName, controller, ...)
  end
end

if LUI and LUI.FlowManager and type(LUI.FlowManager.RequestPopupMenu) == "function" then
  local originalReqPopup = LUI.FlowManager.RequestPopupMenu
  LUI.FlowManager.RequestPopupMenu = function(a, b, ...)
    if a == "Social_Main" or b == "Social_Main" then
      local ctrl = resolveController(a, b)
      return LUI.FlowManager.RequestAddMenu(ctrl, "AlterSocialMenu", true, nil)
    end
    return originalReqPopup(a, b, ...)
  end
end

if LUI and LUI.FlowManager and type(LUI.FlowManager.RequestAddMenu) == "function" then
  local originalAddMenu = LUI.FlowManager.RequestAddMenu
  LUI.FlowManager.RequestAddMenu = function(controller, menuName, ...)
    if menuName == "Social_Main" then
      return originalAddMenu(controller, "AlterSocialMenu", ...)
    end
    return originalAddMenu(controller, menuName, ...)
  end
end

LUI.createMenu.Social_Main = function(controller)
  return LUI.createMenu.AlterSocialMenu(controller)
end
