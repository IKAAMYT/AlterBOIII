-- AlterBO3 (IKAAM) — Menu Social custom (version stylisee + scroll)
--
-- Remplace le menu social natif "Social_Main" (qui crashe : services Live morts)
-- par notre propre menu LUI. Lit la liste d'amis via les fonctions natives du
-- client (game.getfriendcount / game.getfriend / game.connecttofriend).
--
-- Charge automatiquement depuis data/ui_scripts/ (drop-in, pas de recompilation).

if Engine.GetCurrentMap() ~= "core_frontend" then
  return
end

if not LUI or not LUI.createMenu or not CoD or not CoD.Menu then
  return
end

local ACCENT = { 0.949, 0.769, 0.067 }
local COL_TEXT = { 0.96, 0.95, 0.93 }
local COL_DIM = { 0.62, 0.6, 0.55 }
local FONT_TITLE = "fonts/RefrigeratorDeluxe-Regular.ttf"
local FONT_BODY = "fonts/default.ttf"

-- Zone de liste (coordonnees verticales de la fenetre visible)
local LIST_TOP = 150
local LIST_BOTTOM = 660
local CARD_HEIGHT = 56
local CARD_GAP = 8
local ROW_STEP = CARD_HEIGHT + CARD_GAP

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

local function makeFriendCard(controller, friend, top)
  local card = LUI.UIElement.new()
  card:setLeftRight(true, false, 0, 660)
  card:setTopBottom(true, false, top, top + CARD_HEIGHT)
  card:makeFocusable()
  card:setHandleMouse(true)

  local bg = LUI.UIImage.new()
  bg:setLeftRight(true, true, 0, 0)
  bg:setTopBottom(true, true, 0, 0)
  bg:setRGB(0.1, 0.095, 0.08)
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
  dot:setLeftRight(true, false, 22, 32)
  dot:setTopBottom(true, false, 23, 33)
  dot:setRGB(dotColor[1], dotColor[2], dotColor[3])
  card:addElement(dot)

  local nameText = LUI.UIText.new()
  nameText:setLeftRight(true, false, 48, 420)
  nameText:setTopBottom(true, false, 14, 42)
  nameText:setText(friend.name or "?")
  safeFont(nameText, FONT_BODY)
  nameText:setRGB(COL_TEXT[1], COL_TEXT[2], COL_TEXT[3])
  nameText:setAlignment(LUI.Alignment.Left)
  card:addElement(nameText)

  local stateText = LUI.UIText.new()
  stateText:setLeftRight(false, true, -260, -20)
  stateText:setTopBottom(true, false, 16, 42)
  stateText:setText(label)
  safeFont(stateText, FONT_BODY)
  stateText:setRGB(dotColor[1], dotColor[2], dotColor[3])
  stateText:setAlignment(LUI.Alignment.Right)
  card:addElement(stateText)

  card:registerEventHandler("mouseenter", function()
    bg:setRGB(0.16, 0.15, 0.12)
  end)
  card:registerEventHandler("mouseleave", function()
    bg:setRGB(0.1, 0.095, 0.08)
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
    -- Fenetre de liste clippee (masque le debordement) + conteneur scrollable
    local viewport = LUI.UIElement.new()
    viewport:setLeftRight(true, false, 60, 740)
    viewport:setTopBottom(true, false, LIST_TOP, LIST_BOTTOM)
    viewport:setUseStencil(true)
    self:addElement(viewport)

    local content = LUI.UIElement.new()
    content:setLeftRight(true, false, 0, 680)
    content:setTopBottom(true, false, 0, count * ROW_STEP)
    viewport:addElement(content)

    local top = 0
    for i = 0, count - 1 do
      local friend = nil
      pcall(function()
        friend = game.getfriend(i)
      end)
      if friend then
        content:addElement(makeFriendCard(controller, friend, top))
        top = top + ROW_STEP
      end
    end

    -- Etat de scroll
    local viewHeight = LIST_BOTTOM - LIST_TOP
    local contentHeight = count * ROW_STEP
    local maxScroll = math.max(0, contentHeight - viewHeight)
    self.scrollOffset = 0

    local function applyScroll()
      content:setTopBottom(true, false, -self.scrollOffset, -self.scrollOffset + contentHeight)
    end

    local function scrollBy(delta)
      self.scrollOffset = self.scrollOffset + delta
      if self.scrollOffset < 0 then
        self.scrollOffset = 0
      end
      if self.scrollOffset > maxScroll then
        self.scrollOffset = maxScroll
      end
      applyScroll()
    end

    -- Molette souris
    self:registerEventHandler("scrollup", function()
      scrollBy(-ROW_STEP)
      return true
    end)
    self:registerEventHandler("scrolldown", function()
      scrollBy(ROW_STEP)
      return true
    end)
    -- Certaines versions nomment l'event differemment
    self:registerEventHandler("mousewheel", function(element, event)
      if event and event.delta then
        scrollBy(event.delta > 0 and -ROW_STEP or ROW_STEP)
      end
      return true
    end)

    -- Fleches haut / bas (clavier / manette)
    self:AddButtonCallbackFunction(self, controller, Enum.LUIButton.LUI_KEY_UP, nil, function()
      scrollBy(-ROW_STEP)
      return true
    end, nil, false)
    self:AddButtonCallbackFunction(self, controller, Enum.LUIButton.LUI_KEY_DOWN, nil, function()
      scrollBy(ROW_STEP)
      return true
    end, nil, false)

    -- Indicateur de scroll (petite barre a droite) si contenu deborde
    if maxScroll > 0 then
      local hint = LUI.UIText.new()
      hint:setLeftRight(false, true, -260, -60)
      hint:setTopBottom(true, false, 112, 138)
      hint:setText("Molette / fleches pour defiler")
      safeFont(hint, FONT_BODY)
      hint:setRGB(COL_DIM[1], COL_DIM[2], COL_DIM[3])
      hint:setAlignment(LUI.Alignment.Right)
      self:addElement(hint)
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
