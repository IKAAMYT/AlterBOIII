-- AlterBO3 (IKAAM) — Menu Social custom
--
-- Le menu social natif de BO3 ("Social_Main") tente de contacter les services
-- Xbox Live / Demonware de Microsoft qui n'existent plus, et crashe instantanement
-- au clic (fermeture seche, sans dump). On le remplace donc par notre propre menu
-- LUI qui lit la liste d'amis via les fonctions natives exposees par le client
-- (game.getfriendcount / game.getfriend / game.connecttofriend, etc.).
--
-- Ce fichier est charge automatiquement depuis data/ui_scripts/ (drop-in, pas de
-- recompilation). Il fonctionne uniquement dans le frontend (menu principal).

if Engine.GetCurrentMap() ~= "core_frontend" then
  return
end

-- Petite securite : ne rien casser si l'environnement LUI n'est pas pret.
if not LUI or not LUI.createMenu or not CoD or not CoD.Menu then
  return
end

local ALTER_ACCENT = { 0.949, 0.769, 0.067 } -- #f2c411 (jaune MW IKAAM)

-- Etat -> libelle FR
local function stateLabel(state)
  if state == 2 then
    return "En partie", { 0.4, 0.9, 0.4 }
  elseif state == 1 then
    return "En ligne", { 0.4, 0.8, 1.0 }
  end
  return "Hors ligne", { 0.5, 0.5, 0.5 }
end

-- Construit une ligne d'ami (nom + statut + action rejoindre)
local function makeFriendRow(menu, controller, friend, y)
  local row = LUI.UIElement.new()
  row:setLeftRight(true, false, 40, 700)
  row:setTopBottom(true, false, y, y + 46)
  row:makeFocusable()
  row:setHandleMouse(true)

  local bg = LUI.UIImage.new()
  bg:setLeftRight(true, true, 0, 0)
  bg:setTopBottom(true, true, 0, 0)
  bg:setRGB(0.12, 0.11, 0.09)
  bg:setAlpha(0.75)
  row:addElement(bg)

  -- Nom
  local nameLabel = LUI.UIText.new()
  nameLabel:setLeftRight(true, false, 16, 400)
  nameLabel:setTopBottom(true, false, 6, 34)
  nameLabel:setText(friend.name or "?")
  nameLabel:setTTF("fonts/RajdhaniBold.ttf")
  nameLabel:setRGB(0.96, 0.95, 0.93)
  nameLabel:setAlignment(LUI.Alignment.Left)
  row:addElement(nameLabel)

  -- Statut
  local label, color = stateLabel(friend.status)
  local stateText = LUI.UIText.new()
  stateText:setLeftRight(true, false, 420, 620)
  stateText:setTopBottom(true, false, 10, 34)
  stateText:setText(label)
  stateText:setTTF("fonts/Rajdhani.ttf")
  stateText:setRGB(color[1], color[2], color[3])
  stateText:setAlignment(LUI.Alignment.Left)
  row:addElement(stateText)

  -- Clic gauche -> rejoindre si en partie
  if CoD.isPC then
    row:registerEventHandler("leftmousedown", function()
      if friend.status == 2 and friend.steam_id and friend.steam_id ~= "0" then
        pcall(function()
          game.connecttofriend(friend.steam_id)
        end)
      end
    end)
  end

  return row
end

LUI.createMenu.AlterSocialMenu = function(controller)
  local self = CoD.Menu.NewForUIEditor("AlterSocialMenu")
  if PreLoadFunc then
    PreLoadFunc(self, controller)
  end
  self.soundSet = "ChooseDecal"
  self:setOwner(controller)
  self:setLeftRight(true, true, 0, 0)
  self:setTopBottom(true, true, 0, 0)
  self:playSound("menu_open", controller)
  self.buttonModel =
    Engine.CreateModel(Engine.GetModelForController(controller), "AlterSocialMenu.buttonPrompts")
  self.anyChildUsesUpdateState = true

  -- Fond reutilise du jeu (evite de recreer un theme complet)
  local background = CoD.GameSettings_Background.new(self, controller)
  background:setLeftRight(true, true, 0, 0)
  background:setTopBottom(true, true, 0, 0)
  pcall(function()
    background.MenuFrame.titleLabel:setText(Engine.Localize("AMIS"))
    background.MenuFrame.cac3dTitleIntermediary0.FE3dTitleContainer0.MenuTitle.TextBox1.Label0:setText(
      Engine.Localize("AMIS")
    )
  end)
  self:addElement(background)
  self.background = background

  -- Conteneur scrollable simple pour les amis
  local list = LUI.UIElement.new()
  list:setLeftRight(true, false, 0, 760)
  list:setTopBottom(true, false, 150, 700)
  self:addElement(list)
  self.list = list

  -- Remplissage de la liste
  local ok, count = pcall(function()
    return game.getfriendcount()
  end)
  if not ok or type(count) ~= "number" then
    count = 0
  end

  if count == 0 then
    local empty = LUI.UIText.new()
    empty:setLeftRight(true, false, 40, 700)
    empty:setTopBottom(true, false, 40, 70)
    empty:setText(Engine.Localize("Aucun ami pour le moment. Ajoute-les depuis le launcher !"))
    empty:setTTF("fonts/Rajdhani.ttf")
    empty:setRGB(0.7, 0.68, 0.62)
    empty:setAlignment(LUI.Alignment.Left)
    list:addElement(empty)
  else
    local y = 0
    for i = 0, count - 1 do
      local gotFriend, friend = pcall(function()
        return game.getfriend(i)
      end)
      if gotFriend and friend then
        local row = makeFriendRow(self, controller, friend, y)
        list:addElement(row)
        y = y + 52
      end
    end
  end

  -- Bouton retour (croix / B)
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

  pcall(function()
    background.MenuFrame:setModel(self.buttonModel, controller)
  end)

  self:processEvent({ name = "menu_loaded", controller = controller })
  self:processEvent({ name = "update_state", menu = self })

  LUI.OverrideFunction_CallOriginalSecond(self, "close", function(element)
    if element.background then
      element.background:close()
    end
    Engine.UnsubscribeAndFreeModel(
      Engine.GetModel(Engine.GetModelForController(controller), "AlterSocialMenu.buttonPrompts")
    )
  end)

  return self
end

-- L'ouverture de ce menu est declenchee directement depuis le bouton "Social"
-- du menu principal (voir frontend_menus/datasources_gamesettingsflyout_buttons.lua),
-- qui appelle desormais AlterSocialMenu au lieu du "Social_Main" natif qui crashe.
