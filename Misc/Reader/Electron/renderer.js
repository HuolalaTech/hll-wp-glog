document.addEventListener('drop', (e) => {
  e.preventDefault();
  e.stopPropagation();

  let filePaths = new Array()
  for (const f of e.dataTransfer.files) {
    console.log('File(s) you dragged here: ', f.path)
    filePaths.push(f.path)
  }

  let needDecode = document.getElementById("protobuf").checked
  // console.log("pb checked:", document.getElementById("protobuf").checked)
  // console.log("plain checked:", document.getElementById("plain").checked)

  window.electron.dragOver(filePaths, needDecode)
});
document.addEventListener('dragover', (e) => {
  e.preventDefault();
  e.stopPropagation();
});