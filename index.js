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
      