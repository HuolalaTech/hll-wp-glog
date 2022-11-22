const { app, BrowserWindow, ipcMain, dialog } = require('electron')
const path = require('path')
const fs = require('fs')
const gl = require('./glog_reader.js')
const pb = require('./Log_pb.js')
const ProgressBar = require('electron-progressbar');

const log = require('electron-log');
Object.assign(console, log.functions);

var dateFormat = require('dateformat');

function createWindow() {
  const win = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
    }
  })

  win.loadFile('index.html')

  // win.webContents.openDevTools()
}

function createProgressBar() {
  var progressBar = new ProgressBar({
    title: 'Glog Reader',
    text: '正在解析数据',
    detail: '解析中...',
    browserWindow: {
      webPreferences: {
        contextIsolation: false
      }
    }
  })

  progressBar
    .on('completed', function () {
      console.info(`完成`)
      progressBar.detail = '解析完成'
    })
    .on('aborted', function () {
      console.info(`aborted...`)
    })

  // setTimeout(function () {
  //   progressBar.setCompleted()
  // }, 3000)

  return progressBar
}

app.whenReady().then(() => {
  createWindow()

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow()
    }
  })
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit()
  }
})

ipcMain.on('ondragover', (event, files, needDecode) => {
  console.log("drag in files", files)
  console.log("need decode", needDecode)

  if (files.length > 0) {
    for (const f of files) {
      let ext = path.extname(f)
      if (ext !== '.glog' && ext !== '.glogmmap') {
        dialog.showErrorBox("非法的 Glog 文件:" + f, "文件后缀必须是 .glog 或者 .glogmmap")
        return
      }
    }

    dialog.showOpenDialog({
      message: "选择保存文件目录", buttonLabel: "确定", properties: ['openDirectory']
    }).then((result) => {
      if (result && !result.canceled) {
        console.log("choose dir", result.filePaths[0])

        let loading = createProgressBar()

        setTimeout(function () {
          parseLogFiles(files, result.filePaths[0], needDecode)
          loading.setCompleted()
        }, 500) // HACK: wait for loading start
      }
    })
  }
})

function parseLogFiles(files, outputDir, needDecode) {
  let brokenFiles = new Array()

  for (const f of files) {
    let outputFile = path.join(outputDir, (path.extname(f) === '.glog' ? path.basename(f, "glog") : path.basename(f, "glogmmap")) + "json")
    let buf = fs.readFileSync(f)

    console.log("outputFile", outputFile)

    if (fs.existsSync(outputFile)) {
      fs.unlinkSync(outputFile)
      console.log("outputFile [%s] exists, remove it", outputFile)
    }

    try {
      let key = "9C8B23A406216B7B93AA94C66AA5451CCE41DD57A8D5ADBCE8D9F1E7F3D33F45"
      let glogReader = gl.createGlogReader(buf, key)

      let total = 0
      var start = new Date().getTime()
      for (; ;) {
        const pair = glogReader.read()
        const n = pair[0];
        const data = pair[1];
        if (n < 0) {
          break;
        } else if (n == 0) {
          if (!brokenFiles.includes(f)) {
            brokenFiles.push(f)
          }
          continue;
        } else if (n > 0) { // in case n == undefined
          total++;
          if (needDecode) {
            let pbObject = proto.glog.Log.deserializeBinary(data)

            // timestamp [1621406790143] to str [2021-05-19 14:46:30.143]
            let formatted = dateFormat(new Number(pbObject.getTimestamp()), 'yyyy-mm-dd HH:MM:ss.l')
            let level = "Info"
            switch (pbObject.getLoglevel()) {
              case 0:
                level = "Info"
                break
              case 1:
                level = "Warn"
                break
              case 2:
                level = "Error"
                break
              case 3:
                level = "Verbose"
                break
              case 4:
                level = "Debug"
                break
            }
            // console.log("timestamp [%s] to str [%s]", pbObject.getTimestamp(), formatted)

            pbObject.setTimestamp(formatted)
            pbObject.setLoglevel(level)
            fs.appendFileSync(outputFile, JSON.stringify(pbObject.toObject()) + "\n")
          } else {
            fs.appendFileSync(outputFile, new TextDecoder().decode(data) + "\n")
          }
        }
      }
      var end = new Date().getTime()
      var time = end - start
      console.log('read glog file [%s] cost:%d millis, total log num:%d', f, time, total)
    } catch (error) {
      console.error("file [%s] parse error:", f, error)
      dialog.showErrorBox("文件 [" + f + "] 解析错误, 确认是否需要使用 protobuf 格式解析", error.toString())
    }
  }

  if (brokenFiles && brokenFiles.length > 0) {
    dialog.showErrorBox("文件损坏提示", "文件 " + brokenFiles + " 可能损坏. 别担心, GlogReader 会尝试跳过错误部分")
  }
}

