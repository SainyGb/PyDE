let pythonScript = document.getElementById('message');
let output = document.getElementById('output');
const api = () =>{
  console.log(pythonScript.value);
  fetch("http://127.0.0.1:51482/", {
    method: "POST",
    headers: {
      "Content-Type": "text/plain",
    },
    body: pythonScript.value  // raw text only
  })
      .then(response => {
          response.text().then(data => {
          console.log(data);
          output.innerText=data;})
    })
      .catch(error => {console.error('Error:', error)
        output.innerText = "Error: " + error;
      });}

const baixarArquivo = () => {
  const blob = new Blob([pythonScript.value], { type: 'text/plain' });
  const fileURL = URL.createObjectURL(blob);
  const downloadLink = document.createElement('a');

  downloadLink.href = fileURL;

  downloadLink.download = 'script.py';

  document.body.appendChild(downloadLink);

  downloadLink.click();
  URL.revokeObjectURL(fileURL);
}
      