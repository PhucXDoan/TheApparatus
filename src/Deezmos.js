let dummy_input = document.createElement("input");
dummy_input.type = "file";
dummy_input.onchange = ({ target: { files: [dummy_blob] } }) =>
{
	let dummy_reader = new FileReader();
	dummy_reader.readAsText(dummy_blob, "UTF-8");
	dummy_reader.onload = ({ target: { result: file_content }}) =>
	{
		let json  = JSON.parse(file_content);
		let state = Calc.getState();

		if ("avg_screenshot_rgbs" in json)
		{
			const collection_folder_id = Math.random().toString();

			state.graph.squareAxes = false;
			state.graph.viewport.xmin = -0.5;
			state.graph.viewport.ymin = -0.1;
			state.graph.viewport.xmax = json.avg_screenshot_rgbs.length + 1.5;
			state.graph.viewport.ymax = 1.1;

			state.expressions =
				{
				list:
					[
						{
							type  : "expression",
							latex : `C_1 = \\left[\\operatorname{rgb}\\left(255, 0, 0\\right), \\operatorname{rgb}\\left(30, 150, 30\\right), \\operatorname{rgb}\\left(0, 0, 255\\right)\\right]`
						},
						{
							type      : "folder",
							title     : "Collection 1",
							collapsed : true,
							id        : collection_folder_id,
						},
						...['R', 'G', 'B'].flatMap((channel_name, channel_index) => ([
							{
								type       : "expression",
								folderId   : collection_folder_id,
								colorLatex : `C_1\\left[${channel_index + 1}\\right]`,
								latex      : `y = \\operatorname{mean}\\left(${channel_name}\\right)`
							},
							...json.avg_screenshot_rgbs.flatMap((screenshot, screenshot_index) => ({
								type             : "expression",
								colorLatex       : `C_1\\left[${channel_index + 1}\\right]`,
								label            : screenshot.name,
								labelSize        : "0.6",
								labelOrientation : screenshot_index % 2 ? "above" : "below",
								showLabel        : true,
								folderId         : collection_folder_id,
								latex            : `\\left(${screenshot_index + 1}, ${channel_name}\\left[${screenshot_index + 1}\\right]\\right)`
							}))
						])),
						{
							type     : "table",
							folderId : collection_folder_id,
							columns  :
							[
								{ latex: "I", values: json.avg_screenshot_rgbs.map((_, i) => (i + 1).toString()) },
								{ latex: "R", values: json.avg_screenshot_rgbs.map(screenshot => screenshot.avg_rgb.r.toString()), hidden: true },
								{ latex: "G", values: json.avg_screenshot_rgbs.map(screenshot => screenshot.avg_rgb.g.toString()), hidden: true },
								{ latex: "B", values: json.avg_screenshot_rgbs.map(screenshot => screenshot.avg_rgb.b.toString()), hidden: true },
							]
						},
					]
			};
		}

		Calc.setState(state);
	};
};
setTimeout(() => document.querySelector("div.align-right-container").appendChild(dummy_input), 500);
