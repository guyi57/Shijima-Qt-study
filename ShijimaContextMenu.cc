// 
// Shijima-Qt - Cross-platform shimeji simulation app for desktop
// Copyright (C) 2025 pixelomer
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
// 

#include "ShijimaContextMenu.hpp"
#include "ShijimaWidget.hpp"
#include "ShijimaManager.hpp"
#include <QMap>

// 行为名称中文翻译映射表
static QString translateBehaviorName(const std::string &name) {
    static QMap<QString, QString> translations = {
        // 基础行为
        {"Fall", "下落"},
        {"Dragged", "被拖拽"},
        {"Thrown", "被投掷"},
        {"ChaseMouse", "追逐鼠标"},
        
        // 坐姿相关
        {"SitDown", "坐下"},
        {"SitAndFaceMouse", "坐着面向鼠标"},
        {"SitAndSpinHead", "坐着转头"},
        {"SitWhileDanglingLegs", "坐着晃腿"},
        {"StandUp", "站起来"},
        
        // 躺卧相关
        {"LieDown", "躺下"},
        
        // 行走相关
        {"WalkAlongWorkAreaFloor", "沿地板行走"},
        {"WalkLeftAlongFloorAndSit", "向左走并坐下"},
        {"WalkRightAlongFloorAndSit", "向右走并坐下"},
        {"WalkLeftAndSit", "向左走并坐下"},
        {"WalkRightAndSit", "向右走并坐下"},
        {"WalkAndGrabBottomLeftWall", "走到左墙边"},
        {"WalkAndGrabBottomRightWall", "走到右墙边"},
        {"RunAlongWorkAreaFloor", "沿地板奔跑"},
        
        // 爬行相关
        {"CrawlAlongWorkAreaFloor", "沿地板爬行"},
        {"CrawlAlongIECeiling", "沿天花板爬行"},
        
        // 攀爬相关
        {"ClimbIEWall", "爬墙"},
        {"HoldOntoWall", "抓住墙壁"},
        {"FallFromWall", "从墙上掉落"},
        {"HoldOntoCeiling", "抓住天花板"},
        {"FallFromCeiling", "从天花板掉落"},
        {"GrabWorkAreaBottomLeftWall", "抓住左下角"},
        {"GrabWorkAreaBottomRightWall", "抓住右下角"},
        
        // 跳跃相关
        {"JumpFromBottomOfIE", "从底部跳起"},
        
        // 繁殖相关
        {"SplitIntoTwo", "分裂成两个"},
        {"PullUpShimeji", "拉起桌宠"},
        {"PullUp", "被拉起"},
        {"Divided", "被分裂"},
        
        // 其他行为
        {"Yawn", "打哈欠"},
        {"Sleep", "睡觉"},
        {"Wave", "挥手"},
        {"Dance", "跳舞"},
        {"Jump", "跳跃"},
        {"Spin", "旋转"},
    };
    
    QString qname = QString::fromStdString(name);
    return translations.value(qname, qname);
}

ShijimaContextMenu::ShijimaContextMenu(ShijimaWidget *parent)
    : QMenu("右键菜单", parent)
{
    QAction *action;

    // Behaviors menu   
    {
        std::vector<std::string> behaviors;
        auto &list = parent->m_mascot->initial_behavior_list();
        auto flat = list.flatten_unconditional();
        for (auto &behavior : flat) {
            if (!behavior->hidden) {
                behaviors.push_back(behavior->name);
            }
        }
        auto behaviorsMenu = addMenu("行为");
        for (std::string &behavior : behaviors) {
            QString displayName = translateBehaviorName(behavior);
            action = behaviorsMenu->addAction(displayName);
            connect(action, &QAction::triggered, [this, behavior](){
                shijimaParent()->m_mascot->next_behavior(behavior);
            });
        }
    }

    // Show manager
    action = addAction("显示管理器");
    connect(action, &QAction::triggered, [](){
        ShijimaManager::defaultManager()->setManagerVisible(true);
    });

    // Inspect
    action = addAction("检查器");
    connect(action, &QAction::triggered, [this](){
        shijimaParent()->showInspector();
    });

    // Pause checkbox
    action = addAction("暂停");
    action->setCheckable(true);
    action->setChecked(parent->m_paused);
    connect(action, &QAction::triggered, [this](bool checked){
        shijimaParent()->m_paused = checked;
    });

    // Call another
    action = addAction("召唤同伴");
    connect(action, &QAction::triggered, [this](){
        ShijimaManager::defaultManager()->spawn(this->shijimaParent()->mascotName()
            .toStdString());
    });

    // Dismiss all but one
    action = addAction("只保留一个");
    connect(action, &QAction::triggered, [this](){
        ShijimaManager::defaultManager()->killAllButOne(this->shijimaParent());
    });

    // Dismiss all
    action = addAction("全部关闭");
    connect(action, &QAction::triggered, [](){
        ShijimaManager::defaultManager()->killAll();
    });

    // Dismiss
    action = addAction("关闭");
    connect(action, &QAction::triggered, parent, &ShijimaWidget::closeAction);
}

void ShijimaContextMenu::closeEvent(QCloseEvent *event) {
    shijimaParent()->contextMenuClosed(event);
    QMenu::closeEvent(event);
}

/*
ShijimaContextMenu::~ShijimaContextMenu() {
    auto allActions = actions();
    for (QAction *action : allActions) {
        removeAction(action);
        delete action;
    }
}
*/