const { contextBridge, ipcRenderer } = require('electron')
const path = require('path')

contextBridge.exposeInMainWorld('electron', {
  dragOver: (files, needDecode) => {
    ipcRenderer.send('ondragover', files, needDecode)
  }
})