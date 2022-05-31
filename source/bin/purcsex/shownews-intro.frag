    <h1 class="display-4 fw-bold">在模态框中展示新闻</h1>
    <div class="col-lg-6 mx-auto">
      <p class="lead mb-4">这个示例展示了如何在 xGUI Pro 中引用内嵌的 Bootstrap 前端框架功能</p>
      <p>主要步骤：</p>
      <ol class="list-unstyled">
        <li>端点（HVML 解释器）通过 PURCMC 协议连接到 xGUI Pro。</li>
        <li>端点向 xGUI Pro 发送请求创建本窗口。</li>
        <li>端点将页面的初始内容（静态部分）发送到 xGUI Pro；其中“显示新闻”按钮初始时为禁用状态。</li>
        <li>端点将新闻内容发送到 xGUI Pro；xGUI Pro 将新闻内容添加到页面文档的相应位置。</li>
        <li>端点使能“显示新闻”按钮。</li>
        <li>用户点击“显示新闻”按钮，Bootstrap 将自动展示模态框内容。</li>
        <li>用户点击页面底部的“关闭本示例”按钮后，端点收到 `click` 事件。</li>
        <li>端点向 xGUI Pro 发送销毁窗口的请求。</li>
        <li>窗口消失，端点退出程序执行。</li>
      </ol>
      <div class="d-grid gap-2 d-sm-flex justify-content-sm-center mb-5">
        <button hvml-handle="A1" id="btnShowModal" type="button" class="btn btn-primary" data-bs-toggle="modal" data-bs-target="#exampleModal" disabled>显示新闻</button>
        <button type="button" class="btn btn-outline-secondary btn-lg px-4" disabled>不可用</button>
      </div>
    </div>
