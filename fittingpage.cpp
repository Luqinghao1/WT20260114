/*
 * 文件名: fittingpage.cpp
 * 文件作用: 拟合页面容器类实现文件
 * 功能描述:
 * 1. 实现了多页签管理逻辑（增删改）。
 * 2. 负责将全局的模型管理器和数据模型集合分发给具体的拟合子控件。
 * 3. 实现了拟合状态的序列化与反序列化，支持项目保存恢复。
 * 4. 适配多文件数据源，确保子控件能获取到所有可选的数据文件。
 */

#include "fittingpage.h"
#include "ui_fittingpage.h"
#include "wt_fittingwidget.h"
#include "modelparameter.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonArray>
#include <QDebug>

// 构造函数
FittingPage::FittingPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FittingPage),
    m_modelManager(nullptr)
{
    ui->setupUi(this);
}

// 析构函数
FittingPage::~FittingPage()
{
    delete ui;
}

// 设置模型管理器，并分发给所有现有子页签
void FittingPage::setModelManager(ModelManager *m)
{
    m_modelManager = m;
    // 遍历当前所有页签，更新其模型管理器引用
    for(int i = 0; i < ui->tabWidget->count(); ++i) {
        FittingWidget* w = qobject_cast<FittingWidget*>(ui->tabWidget->widget(i));
        if(w) w->setModelManager(m);
    }
}

// 设置项目数据模型集合，并分发给所有现有子页签
void FittingPage::setProjectDataModels(const QMap<QString, QStandardItemModel*> &models)
{
    m_dataMap = models;
    // 遍历当前所有页签，更新其数据模型引用
    for(int i = 0; i < ui->tabWidget->count(); ++i) {
        FittingWidget* w = qobject_cast<FittingWidget*>(ui->tabWidget->widget(i));
        if(w) {
            // 注意：FittingWidget 需要实现 setProjectDataModels 接口来接收 Map
            w->setProjectDataModels(models);
        }
    }
}

// 将观测数据设置到当前激活页签，若无则自动创建
void FittingPage::setObservedDataToCurrent(const QVector<double> &t, const QVector<double> &p, const QVector<double> &d)
{
    FittingWidget* current = qobject_cast<FittingWidget*>(ui->tabWidget->currentWidget());
    if (current) {
        current->setObservedData(t, p, d);
    } else {
        // 如果当前没有页签，先创建一个
        on_btnNewAnalysis_clicked();
        current = qobject_cast<FittingWidget*>(ui->tabWidget->currentWidget());
        if(current) current->setObservedData(t, p, d);
    }
}

// 更新所有子页签的基本参数
void FittingPage::updateBasicParameters()
{
    for(int i = 0; i < ui->tabWidget->count(); ++i) {
        FittingWidget* w = qobject_cast<FittingWidget*>(ui->tabWidget->widget(i));
        if(w) w->updateBasicParameters();
    }
}

// 创建新页签并初始化
FittingWidget* FittingPage::createNewTab(const QString &name, const QJsonObject &initData)
{
    FittingWidget* w = new FittingWidget(this);

    // 注入依赖：模型管理器
    if(m_modelManager) w->setModelManager(m_modelManager);

    // 注入依赖：所有项目数据模型
    // 即使 Map 为空也传递，确保指针状态正确
    w->setProjectDataModels(m_dataMap);

    // 连接保存请求信号
    connect(w, &FittingWidget::sigRequestSave, this, &FittingPage::onChildRequestSave);

    // 添加到 TabWidget 并选中
    int index = ui->tabWidget->addTab(w, name);
    ui->tabWidget->setCurrentIndex(index);

    // 如果有初始状态数据（如从文件加载或复制），则进行加载
    if(!initData.isEmpty()) {
        w->loadFittingState(initData);
    }

    return w;
}

// 生成唯一的页签名称（如 "Analysis 1", "Analysis 2"）
QString FittingPage::generateUniqueName(const QString &baseName)
{
    QString name = baseName;
    int counter = 1;
    bool exists = true;
    while(exists) {
        exists = false;
        for(int i=0; i<ui->tabWidget->count(); ++i) {
            if(ui->tabWidget->tabText(i) == name) {
                exists = true;
                break;
            }
        }
        if(exists) {
            counter++;
            name = QString("%1 %2").arg(baseName).arg(counter);
        }
    }
    return name;
}

// 新建分析按钮槽函数
void FittingPage::on_btnNewAnalysis_clicked()
{
    QStringList items;
    items << "空白分析 (Blank)";
    // 允许从现有分析复制
    for(int i=0; i<ui->tabWidget->count(); ++i) {
        items << "复制: " + ui->tabWidget->tabText(i);
    }

    bool ok;
    QString item = QInputDialog::getItem(this, "新建分析", "请选择创建方式:", items, 0, false, &ok);
    if (!ok || item.isEmpty()) return;

    QString newName = generateUniqueName("Analysis");

    if (item == "空白分析 (Blank)") {
        createNewTab(newName);
    } else {
        // 复制现有状态
        int indexToCopy = items.indexOf(item) - 1;
        FittingWidget* source = qobject_cast<FittingWidget*>(ui->tabWidget->widget(indexToCopy));
        if(source) {
            QJsonObject state = source->getJsonState();
            createNewTab(newName, state);
        }
    }
}

// 重命名按钮槽函数
void FittingPage::on_btnRenameAnalysis_clicked()
{
    int idx = ui->tabWidget->currentIndex();
    if(idx < 0) return;

    QString oldName = ui->tabWidget->tabText(idx);
    bool ok;
    QString newName = QInputDialog::getText(this, "重命名", "请输入新的分析名称:", QLineEdit::Normal, oldName, &ok);
    if(ok && !newName.isEmpty()) {
        ui->tabWidget->setTabText(idx, newName);
    }
}

// 删除按钮槽函数
void FittingPage::on_btnDeleteAnalysis_clicked()
{
    int idx = ui->tabWidget->currentIndex();
    if(idx < 0) return;

    // 至少保留一个页签
    if(ui->tabWidget->count() == 1) {
        QMessageBox::warning(this, "警告", "至少需要保留一个分析页面！");
        return;
    }

    if(QMessageBox::question(this, "确认", "确定要删除当前分析页吗？\n此操作不可恢复。") == QMessageBox::Yes) {
        QWidget* w = ui->tabWidget->widget(idx);
        ui->tabWidget->removeTab(idx);
        delete w;
    }
}

// 保存所有状态到 ModelParameter
void FittingPage::saveAllFittingStates()
{
    QJsonArray analysesArray;
    for(int i=0; i<ui->tabWidget->count(); ++i) {
        FittingWidget* w = qobject_cast<FittingWidget*>(ui->tabWidget->widget(i));
        if(w) {
            QJsonObject pageObj = w->getJsonState();
            pageObj["_tabName"] = ui->tabWidget->tabText(i);
            analysesArray.append(pageObj);
        }
    }

    QJsonObject root;
    root["version"] = "2.0";
    root["analyses"] = analysesArray;

    ModelParameter::instance()->saveFittingResult(root);
}

// 从 ModelParameter 加载所有状态
void FittingPage::loadAllFittingStates()
{
    QJsonObject root = ModelParameter::instance()->getFittingResult();
    if(root.isEmpty()) {
        if(ui->tabWidget->count() == 0) createNewTab("Analysis 1");
        return;
    }

    ui->tabWidget->clear();

    if(root.contains("analyses") && root["analyses"].isArray()) {
        QJsonArray arr = root["analyses"].toArray();
        for(int i=0; i<arr.size(); ++i) {
            QJsonObject pageObj = arr[i].toObject();
            QString name = pageObj.contains("_tabName") ? pageObj["_tabName"].toString() : QString("Analysis %1").arg(i+1);
            createNewTab(name, pageObj);
        }
    } else {
        // 兼容旧版单一状态配置
        createNewTab("Analysis 1", root);
    }

    // 确保至少有一个页签
    if(ui->tabWidget->count() == 0) createNewTab("Analysis 1");
}

// 响应子页面的保存请求
void FittingPage::onChildRequestSave()
{
    saveAllFittingStates();
    QMessageBox::information(this, "保存成功", "所有分析页的状态已保存到项目文件 (pwt) 中。");
}

// 重置拟合分析功能
void FittingPage::resetAnalysis()
{
    // 1. 循环删除所有页签及其内部的 Widget
    // QTabWidget::clear() 只移除不删除，所以必须手动 delete
    while (ui->tabWidget->count() > 0) {
        QWidget* w = ui->tabWidget->widget(0);
        ui->tabWidget->removeTab(0); // 先从界面移除
        delete w;                    // 再销毁对象
    }

    // 2. 重新创建一个默认的空白分析页，恢复初始状态
    createNewTab("Analysis 1");
}
