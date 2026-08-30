if not Engine.IsInGame() then
  return
end

local isPrivateGame = true
pcall(function()
  if Dvar.cl_connected_to_dedi and Dvar.cl_connected_to_dedi:get() then
    isPrivateGame = false
  end
end)

if isPrivateGame then
  local tweakNames = {
    g_speed = "Vitesse de déplacement",
    bg_gravity = "Gravité",
    player_sustainAmmo = "Munitions infinies",
    timescale = "Vitesse de jeu",
    cg_drawGun = "Weapon Model",
    r_fog = "Fog",
    cg_fov_default = "Champ de vision",
    cg_fovScale = "Échelle FOV",
  }

  local function showTweakToast(dvarName, value)
    pcall(function()
      if CoD.OverlayUtility and CoD.OverlayUtility.ShowToast then
        local label = tweakNames[dvarName] or dvarName
        CoD.OverlayUtility.ShowToast(
          "DefaultState",
          "RÉGLAGES DE JEU",
          label .. ": " .. tostring(value),
          "t7_icon_info_overlays_bkg"
        )
      end
    end)
  end

  local lastTweakValues = {}
  for dvarName, _ in pairs(tweakNames) do
    pcall(function()
      lastTweakValues[dvarName] = tostring(Engine.DvarString(nil, dvarName))
    end)
  end

  local function showTweakToastIfChanged(dvarName, value)
    local normalizedValue = tostring(value)
    local previousValue = lastTweakValues[dvarName]
    lastTweakValues[dvarName] = normalizedValue
    if previousValue ~= nil and previousValue ~= normalizedValue then
      showTweakToast(dvarName, value)
    end
  end

  local function updateDvar(f1_arg0, f1_arg1, f1_arg2, dvarName, f1_arg4)
    UpdateInfoModels(f1_arg1)
    local val = f1_arg1.value
    pcall(function()
      Engine.Exec(f1_arg2, dvarName .. " " .. tostring(val))
    end)
    showTweakToastIfChanged(dvarName, val)
  end

  DataSources.BoiiiGameTweaks = DataSourceHelpers.ListSetup("BoiiiGameTweaks", function(controller)
    local t = {}

    local speedOptions = {}
    local currentSpeed = 190
    pcall(function()
      currentSpeed = Engine.DvarInt(nil, "g_speed")
    end)
    for _, val in ipairs({ 100, 150, 190, 250, 300, 400, 500, 800 }) do
      table.insert(speedOptions, {
        option = val == 190 and "190 (défaut)" or tostring(val),
        value = val,
        default = val == currentSpeed,
      })
    end
    table.insert(
      t,
      CoD.OptionsUtility.CreateDvarSettings(
        controller,
        "Vitesse de déplacement",
        "Vitesse de déplacement du joueur.",
        "GameTweaks_gspeed",
        "g_speed",
        speedOptions,
        nil,
        updateDvar
      )
    )

    local gravityOptions = {}
    local currentGravité = 800
    pcall(function()
      currentGravité = Engine.DvarInt(nil, "bg_gravity")
    end)
    for _, val in ipairs({ 50, 100, 200, 400, 800, 1200, 1600, 2000 }) do
      table.insert(gravityOptions, {
        option = val == 800 and "800 (défaut)" or val <= 100 and val .. " (Moon)" or tostring(val),
        value = val,
        default = val == currentGravité,
      })
    end
    table.insert(
      t,
      CoD.OptionsUtility.CreateDvarSettings(
        controller,
        "Gravité",
        "Faible = flottant, élevée = lourd.",
        "GameTweaks_gravity",
        "bg_gravity",
        gravityOptions,
        nil,
        updateDvar
      )
    )

    table.insert(
      t,
      CoD.OptionsUtility.CreateDvarSettings(
        controller,
        "Munitions infinies",
        "Ne manquez jamais de munitions.",
        "GameTweaks_infiniteammo",
        "player_sustainAmmo",
        {
          { option = "MENU_DISABLED", value = 0, default = true },
          { option = "MENU_ENABLED", value = 1 },
        },
        nil,
        updateDvar
      )
    )

    table.insert(
      t,
      CoD.OptionsUtility.CreateDvarSettings(
        controller,
        "Vitesse de jeu",
        "Accélère ou ralentit le jeu.",
        "GameTweaks_timescale",
        "timescale",
        {
          { option = "0.25x (Ralenti)", value = 0.25 },
          { option = "0.5x (Demi-vitesse)", value = 0.5 },
          { option = "1x (Normal)", value = 1, default = true },
          { option = "1.5x (Rapide)", value = 1.5 },
          { option = "2x (Double)", value = 2 },
          { option = "4x (Chaos)", value = 4 },
        },
        nil,
        updateDvar
      )
    )

    table.insert(
      t,
      CoD.OptionsUtility.CreateDvarSettings(
        controller,
        "Masquer le modèle d'arme",
        "Cache l'arme à l'écran.",
        "GameTweaks_drawgun",
        "cg_drawGun",
        {
          { option = "Affiché (défaut)", value = 1, default = true },
          { option = "Masqué", value = 0 },
        },
        nil,
        updateDvar
      )
    )

    table.insert(
      t,
      CoD.OptionsUtility.CreateDvarSettings(
        controller,
        "Désactiver le brouillard",
        "Supprime les effets de brouillard.",
        "GameTweaks_fog",
        "r_fog",
        {
          { option = "Brouillard activé (défaut)", value = 1, default = true },
          { option = "Brouillard désactivé", value = 0 },
        },
        nil,
        updateDvar
      )
    )

    -- FOV
    local fovOptions = {}
    local currentFov = 80
    pcall(function()
      currentFov = Engine.DvarInt(nil, "cg_fov_default")
    end)
    for fov = 65, 120, 5 do
      table.insert(fovOptions, {
        option = fov == 80 and "80 (défaut)" or tostring(fov),
        value = fov,
        default = fov == currentFov,
      })
    end
    table.insert(
      t,
      CoD.OptionsUtility.CreateDvarSettings(
        controller,
        "Champ de vision",
        "Réglez votre champ de vision (65-120).",
        "GameTweaks_fov",
        "cg_fov_default",
        fovOptions,
        nil,
        updateDvar
      )
    )

    -- Échelle FOV
    local fovScaleOptions = {}
    for _, s in ipairs({ 0.5, 0.6, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95, 1.0, 1.05, 1.1, 1.15, 1.2, 1.5, 2.0 }) do
      table.insert(fovScaleOptions, {
        option = s == 1.0 and "1.0 (défaut)" or tostring(s),
        value = s,
        default = s == 1.0,
      })
    end
    table.insert(
      t,
      CoD.OptionsUtility.CreateDvarSettings(
        controller,
        "Échelle FOV",
        "Multiplicateur du FOV ADS/zoom. 1.0 = défaut.",
        "GameTweaks_fovscale",
        "cg_fovScale",
        fovScaleOptions,
        nil,
        function(f1_arg0, f1_arg1, f1_arg2, dvarName, f1_arg4)
          UpdateInfoModels(f1_arg1)
          Engine.SetDvar(dvarName, f1_arg1.value)
          showTweakToastIfChanged(dvarName, f1_arg1.value)
        end
      )
    )

    return t
  end)

  LUI.createMenu.BoiiiGameTweaksMenu = function(controller)
    pcall(require, "ui.uieditor.widgets.StartMenu.StartMenu_frame_noBG")
    pcall(require, "ui.uieditor.widgets.StartMenu.StartMenu_Options_Slider_Item_Arrow")
    pcall(require, "ui.uieditor.widgets.StartMenu.StartMenu_Options_Slider_Control_Item")
    pcall(require, "ui.uieditor.widgets.Lobby.Common.FE_FocusBarContainer")
    pcall(require, "ui.uieditor.widgets.GameSettings.GameSettings_ChangedIndicator")
    pcall(require, "ui.uieditor.widgets.StartMenu.StartMenu_Options_Slider")

    local SliderWidget = CoD.Slider_Small or CoD.StartMenu_Options_Slider

    local self = CoD.Menu.NewForUIEditor("BoiiiGameTweaksMenu")
    self.soundSet = "ChooseDecal"
    self:setOwner(controller)
    self:setLeftRight(true, true, 0, 0)
    self:setTopBottom(true, true, 0, 0)
    self:playSound("menu_open", controller)
    self.buttonModel = Engine.CreateModel(Engine.GetModelForController(controller), "BoiiiGameTweaksMenu.buttonPrompts")
    self.anyChildUsesUpdateState = true

    local bg = LUI.UIImage.new()
    bg:setLeftRight(true, true, 0, 0)
    bg:setTopBottom(true, true, 0, 0)
    bg:setRGB(0, 0, 0)
    bg:setAlpha(0.85)
    self:addElement(bg)

    local list = LUI.UIList.new(self, controller, 2, 0, nil, false, false, 0, 0, false, false)
    list:makeFocusable()
    list:setLeftRight(true, false, 26, 741)
    list:setTopBottom(true, false, 135, 555)
    if SliderWidget then
      list:setWidgetType(SliderWidget)
    end
    list:setVerticalCount(9)
    list:setDataSource("BoiiiGameTweaks")
    self:addElement(list)
    self.Options = list

    if CoD.GenericMenuFrame then
      local frame = CoD.GenericMenuFrame.new(self, controller)
      frame:setLeftRight(true, true, 0, 0)
      frame:setTopBottom(true, true, 0, 0)
      frame.titleLabel:setText(Engine.Localize("RÉGLAGES DE JEU"))
      pcall(function()
        frame.cac3dTitleIntermediary0.FE3dTitleContainer0.MenuTitle.TextBox1.Label0:setText(
          Engine.Localize("RÉGLAGES DE JEU")
        )
      end)
      frame:setModel(self.buttonModel, controller)
      self:addElement(frame)
      self.MenuFrame = frame
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

    list.id = "Options"

    self:processEvent({ name = "menu_loaded", controller = controller })
    self:processEvent({ name = "update_state", menu = self })
    if not self:restoreState() then
      self.Options:processEvent({ name = "gain_focus", controller = controller })
    end

    LUI.OverrideFunction_CallOriginalSecond(self, "close", function(element)
      if element.MenuFrame then
        element.MenuFrame:close()
      end
      element.Options:close()
      Engine.UnsubscribeAndFreeModel(
        Engine.GetModel(Engine.GetModelForController(controller), "BoiiiGameTweaksMenu.buttonPrompts")
      )
    end)

    return self
  end
end

if not LUI.createMenu.BoiiiGameTweaksMenu then
  LUI.createMenu.BoiiiGameTweaksMenu = function(controller)
    local self = CoD.Menu.NewForUIEditor("BoiiiGameTweaksMenu")
    self.soundSet = "ChooseDecal"
    self:setOwner(controller)
    self:setLeftRight(true, true, 0, 0)
    self:setTopBottom(true, true, 0, 0)
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
    return self
  end
end
