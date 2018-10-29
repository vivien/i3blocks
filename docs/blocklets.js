document.onreadystatechange = function () {
	if (document.readyState === 'complete') {
		var repos = document.querySelector('#repos');
		var request = new XMLHttpRequest();
		request.open('GET', "https://api.github.com/search/repositories?q=topic:i3blocks+topic:blocklet");
		request.onload = function () {
			var data = JSON.parse(request.responseText);

			data.items.forEach(function (repo) {
				repos.innerHTML += `<div class="card repo">
					<div class="card-top-left">
						<a href="${ repo.owner.html_url }"><img class="repo-owner-avatar" src="${ repo.owner.avatar_url }" alt="${ repo.owner.login }" /></a>
					</div>
					<div class="card-top-center">
						<p class="repo-name"><a href="${ repo.html_url }">${ repo.name }</a></p>
						<p class="repo-language">Include ${ repo.language } code</p>
						<p class="repo-last-update">Updated ${ moment(repo.updated_at).fromNow() }</p>
					</div>
					<div class="card-top-right">
						<p class="repo-stars"><span>☆</span>${ repo.stargazers_count }</p>
					</div>
					<div class="card-bottom">
						<p class="repo-desc">${ repo.description }</p>
					</div>
				</div>`;
			});
		}

		request.send();
	}
}
