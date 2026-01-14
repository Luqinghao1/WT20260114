/*
 * 文件名: chartsetting2.h
 * 文件作用: 双坐标图表设置对话框头文件
 * 功能描述:
 * 1. 管理叠加模式（Stacked Mode）下的图表设置。
 * 2. 支持分别设置顶部坐标系（压力）和底部坐标系（产量）的坐标轴参数。
 * 3. [新增] 支持曲线列表管理（样式预览、改名、显隐控制），与 ChartSetting1 保持一致。
 */

#ifndef CHARTSETTING2_H
#define CHARTSETTING2_H

#include <QDialog>
#include "mousezoom.h"
#include "qcustomplot.h"

namespace Ui {
class ChartSetting2;
}

class ChartSetting2 : public QDialog
{
    Q_OBJECT

public:
    explicit ChartSetting2(MouseZoom* plot, QCPTextElement* title,
                           QCPAxisRect* topRect, QCPAxisRect* bottomRect,
                           QWidget *parent = nullptr);
    ~ChartSetting2();

private slots:
    void on_btnOk_clicked();
    void on_btnCancel_clicked();
    void on_btnApply_clicked();

private:
    Ui::ChartSetting2 *ui;
    MouseZoom* m_plot;
    QCPTextElement* m_title;
    QCPAxisRect* m_topRect;
    QCPAxisRect* m_bottomRect;

    void initData();
    void applySettings();
};

#endif // CHARTSETTING2_H
