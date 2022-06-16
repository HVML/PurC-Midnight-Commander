<div class="modal-dialog modal-lg modal-dialog-centered">
    <div class="modal-content">
      <div class="modal-header">
        <h5 class="modal-title" id="exampleModalLabel">主要技术细节</h5>
        <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="关闭"></button>
      </div>
      <div class="modal-body">
        <ol>
          <li>端点（HVML 解释器）通过 PURCMC 协议连接到 xGUI Pro。</li>
          <li>端点向 xGUI Pro 发送请求创建本窗口。</li>
          <li>端点将页面的初始内容（静态部分）发送到 xGUI Pro；其中部分内容初始时显示为占位信息。</li>
          <li>端点将本示例程序的介绍（本内容）发送到 xGUI Pro，替代占位信息；此时“大事件”按钮为禁用状态。</li>
          <li>端点将 HVML 大事件的新闻内容发送到 xGUI Pro；xGUI Pro 将新闻内容添加到页面文档的相应位置。</li>
          <li>端点使能“大事件”按钮。</li>
          <li>用户点击“大事件”按钮，通过 Bootstrap 的功能展示“大事件”的新闻内容。</li>
          <li>用户点击计算器上的精度调节或者数字、加减乘除等按钮后，端点收到相应的 `change` 和 `click` 事件，做必要处理后设置计算器的表达式。</li>
          <li>用户点击计算器上的等于号后，端点收到相应的 `click` 事件，端点调用系统中的 `bc` 程序完成表达式求值，之后将结果展示在计算器界面上。</li>
          <li>用户点击页面底部的“关闭本示例”按钮后，端点收到 `click` 事件。</li>
          <li>端点向 xGUI Pro 发送销毁窗口的请求。</li>
          <li>窗口消失，端点退出程序执行。</li>
        </ol>
      </div>
      <div class="modal-footer">
        <button type="button" class="btn btn-primary" data-bs-dismiss="modal">关闭并送上祝福</button>
      </div>
    </div>
</div>
