// TODO update
const basestr = "http://192.168.178.72:8000";

window.onload = async () => {
  await submit();
};

async function submit() {
  console.log("x called");

  let start = parseInt($("#start-input").val());
  let stop = parseInt($("#stop-input").val());

  var request = new XMLHttpRequest();

  request.onload = function () {
    let json = JSON.parse(request.responseText);

    $("#random-number").text(`${json.code}`);
  };

  request.open("GET", `${basestr}/mixer?start=${start}&stop=${stop}`, true);
  request.send();
}
