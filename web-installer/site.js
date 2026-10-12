// Light/dark toggle shared by the microsite's pages (the choice is kept in localStorage).
function toggleTheme() {
  const light = document.documentElement.getAttribute("data-theme") !== "light";
  if (light) document.documentElement.setAttribute("data-theme", "light");
  else document.documentElement.removeAttribute("data-theme");
  localStorage.setItem("theme", light ? "light" : "dark");
}
