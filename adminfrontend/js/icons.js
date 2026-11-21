/* Minimal inline icon set (24x24, stroke based). Icons.svg(name, size) returns an SVG string. */
(function () {
  var P = {
    dashboard: "M3 3h7v9H3zM14 3h7v5h-7zM14 12h7v9h-7zM3 16h7v5H3z",
    users: "M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2M9 11a4 4 0 1 0 0-8 4 4 0 0 0 0 8M22 21v-2a4 4 0 0 0-3-3.9M16 3.1a4 4 0 0 1 0 7.8",
    faculty: "M3 8h18v12H3zM9 8V5h6v3M3 14h18",
    layers: "M12 3 3 8l9 5 9-5-9-5ZM3 13l9 5 9-5",
    grid: "M3 3h8v8H3zM13 3h8v8h-8zM3 13h8v8H3zM13 13h8v8h-8z",
    calendar: "M4 5h16v16H4zM4 10h16M8 3v4M16 3v4",
    alert: "M12 3 2 20h20L12 3ZM12 10v4M12 17.5v.01",
    transfer: "M7 4v14M3 14l4 4 4-4M17 20V6M13 10l4-4 4 4",
    published: "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18ZM8 12l3 3 5-6",
    list: "M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01",
    sliders: "M4 6h10M18 6h2M4 12h4M12 12h8M4 18h12M20 18h0M16 4v4M10 10v4M18 16v4",
    pencil: "M4 20h4L19 9l-4-4L4 16v4ZM13 7l4 4",
    logout: "M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4M16 17l5-5-5-5M21 12H9",
    menu: "M4 7h16M4 12h16M4 17h16",
    arrow: "M5 12h14M13 6l6 6-6 6",
    chevron: "m9 6 6 6-6 6",
    back: "M19 12H5M11 6l-6 6 6 6",
    play: "M6 4l14 8-14 8V4Z",
    upload: "M12 16V4M7 9l5-5 5 5M4 20h16",
    clock: "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18ZM12 7v5l3 2",
    info: "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18ZM12 11v5M12 8v.01"
  };
  window.Icons = {
    svg: function (name, size) {
      size = size || 20;
      return '<svg viewBox="0 0 24 24" width="' + size + '" height="' + size + '" fill="none" stroke="currentColor" ' +
        'stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="' + (P[name] || "") + '"/></svg>';
    }
  };
})();
